/*
 * main.cpp — Entry point
 *
 * Usage: ./video_sort <input_video.mp4> [--skip-t1] [--threads N]
 *
 * T1: Process video (grayscale + row-sort via QuickSort) → output video.
 * T2: Benchmark 6 scheduler strategies, save results/benchmark_results.txt.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <unistd.h>

extern "C" {
#include "quicksort.h"
#include "metrics.h"
#include "scheduler_bounded_prolific.h"
#include "scheduler_bounded_collective.h"
#include "scheduler_bounded_sko.h"
#include "scheduler_bounded_subreaper_prolific.h"
#include "scheduler_mmap_subreaper_collective.h"
#include "scheduler_mmap_subreaper_collective_barrier_affinity.h"
}

#include "frame_processor.h"

/* ---- helpers ---- */

static double wall_time_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Generate a synthetic batch of n_frames frames (rows x cols) of random data */
static uint8_t *make_synthetic_frames(int n_frames, int rows, int cols)
{
    size_t  sz  = (size_t)n_frames * rows * cols;
    uint8_t *buf = (uint8_t *)malloc(sz);
    if (!buf) { perror("malloc"); exit(EXIT_FAILURE); }
    for (size_t i = 0; i < sz; i++)
        buf[i] = (uint8_t)(rand() & 0xFF);
    return buf;
}

/* Sequential baseline: sort all rows single-threaded */
static double run_sequential(uint8_t *frames,
                              int n_frames, int rows, int cols)
{
    int total_rows = n_frames * rows;
    double t0 = wall_time_now();
    for (int r = 0; r < total_rows; r++) {
        uint8_t *row = frames + (size_t)r * cols;
        if (cols > 1)
            quicksort_row(row, 0, cols - 1);
    }
    return wall_time_now() - t0;
}

/* Reset frame batch to a fixed random state (so each scheduler gets
 * the same unsorted input). */
static void reset_frames(uint8_t *frames, const uint8_t *original,
                         int n_frames, int rows, int cols)
{
    memcpy(frames, original, (size_t)n_frames * rows * cols);
}

/* ---- main ---- */

int main(int argc, char *argv[])
{
    const char *input_video = NULL;
    int         skip_t1     = 0;
    int         n_threads   = (int)sysconf(_SC_NPROCESSORS_ONLN);

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--skip-t1") == 0) {
            skip_t1 = 1;
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            n_threads = atoi(argv[++i]);
            if (n_threads < 1) n_threads = 1;
        } else {
            input_video = argv[i];
        }
    }

    /* ------------------------------------------------------------------ */
    /* T1 — Video frame row sorting                                        */
    /* ------------------------------------------------------------------ */
    if (!skip_t1) {
        if (!input_video) {
            fprintf(stderr,
                    "Usage: %s <input_video> [--skip-t1] [--threads N]\n"
                    "  Use --skip-t1 to skip T1 and run only T2 benchmarks.\n",
                    argv[0]);
            return EXIT_FAILURE;
        }

        /* Build output path: <basename>_sorted.avi */
        char output_path[4096];
        const char *dot = strrchr(input_video, '.');
        const char *sep = strrchr(input_video, '/');
        const char *base = sep ? sep + 1 : input_video;
        size_t base_len = dot ? (size_t)(dot - base) : strlen(base);
        snprintf(output_path, sizeof(output_path),
                 "%.*s_sorted.avi", (int)base_len, base);

        printf("=== T1: Video frame row sorting ===\n");
        printf("Input : %s\n", input_video);
        printf("Output: %s\n\n", output_path);

        if (process_video(input_video, output_path) != 0) {
            fprintf(stderr, "T1 failed.\n");
            return EXIT_FAILURE;
        }
        printf("\nT1 complete.\n\n");
    }

    /* ------------------------------------------------------------------ */
    /* T2 — Scheduler benchmarks                                           */
    /* ------------------------------------------------------------------ */
    printf("=== T2: Scheduler benchmarks ===\n");
    printf("Threads/processes: %d\n\n", n_threads);

    /* Prepare frame batch (synthetic if no video provided) */
    const int N_FRAMES = 100;
    const int ROWS     = 480;
    const int COLS     = 640;

    uint8_t *original = make_synthetic_frames(N_FRAMES, ROWS, COLS);
    uint8_t *frames   = (uint8_t *)malloc((size_t)N_FRAMES * ROWS * COLS);
    if (!frames) { perror("malloc"); exit(EXIT_FAILURE); }

    /* Sequential baseline (measured once, used for all schedulers) */
    reset_frames(frames, original, N_FRAMES, ROWS, COLS);
    printf("Running sequential baseline...\n");
    double T_serial = run_sequential(frames, N_FRAMES, ROWS, COLS);
    printf("  T_serial = %.6f s\n\n", T_serial);

    /* Clamp n_procs for process-based schedulers to avoid excessive forking */
    int n_procs = n_threads;
    if (n_procs > 8) n_procs = 8;

    /* ---- Run each scheduler ---- */
    typedef double (*SchedulerFn)(uint8_t *, int, int, int, int);

    struct {
        const char  *name;
        SchedulerFn  fn;
        int          n;
    } schedulers[] = {
        { "Bounded Prolific",
          run_scheduler_bounded_prolific,                        n_threads },
        { "Bounded Collective",
          run_scheduler_bounded_collective,                      n_threads },
        { "Bounded Sko (Prolific + Collective)",
          run_scheduler_bounded_sko,                             n_threads },
        { "Bounded Subreaper Prolific",
          run_scheduler_bounded_subreaper_prolific,              n_procs   },
        { "Mmap Bounded Subreaper Collective",
          run_scheduler_mmap_subreaper_collective,               n_procs   },
        { "Mmap Bounded Subreaper Collective + Barrier + Affinity",
          run_scheduler_mmap_subreaper_collective_barrier_affinity, n_procs },
    };

    const int N_SCHEDULERS = (int)(sizeof(schedulers) / sizeof(schedulers[0]));
    BenchmarkResult results[N_SCHEDULERS];

    /* Table header */
    printf("| %-46s | %3s | %12s | %14s | %9s | %10s | %11s | %11s |\n",
           "Scheduler", "N", "T_serial (s)", "T_parallel (s)",
           "Speedup", "Efficiency", "Performance", "SEfficiency");
    printf("|:------------------------------------------------|----:|"
           "-------------:|---------------:|----------:|-----------:|"
           "------------:|------------:|\n");

    for (int i = 0; i < N_SCHEDULERS; i++) {
        reset_frames(frames, original, N_FRAMES, ROWS, COLS);

        double T_par = schedulers[i].fn(frames, N_FRAMES, ROWS, COLS,
                                        schedulers[i].n);

        results[i].scheduler_name = schedulers[i].name;
        results[i].T_serial       = T_serial;
        results[i].T_parallel     = T_par;
        results[i].N              = schedulers[i].n;

        compute_metrics(&results[i]);
        print_metrics(&results[i]);
    }

    printf("\n");

    /* Save to file */
    save_metrics_to_file(results, N_SCHEDULERS, "results/benchmark_results.txt");
    printf("Results saved to results/benchmark_results.txt\n");

    free(original);
    free(frames);
    return 0;
}
