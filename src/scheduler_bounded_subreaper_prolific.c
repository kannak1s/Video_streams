/*
 * scheduler_bounded_subreaper_prolific.c — Bounded Subreaper Prolific
 *
 * Strategy:
 *   - Uses fork() (processes, not threads) to spawn workers.
 *   - The parent process calls prctl(PR_SET_CHILD_SUBREAPER, 1), making itself
 *     the subreaper for all descendant processes.
 *   - Worker rows are stored in a shared mmap region (MAP_SHARED|MAP_ANONYMOUS).
 *   - Parent waitpid()s for all children (bounded prolific process tree).
 *   - Worker processes write their output back to shared memory.
 */

#include "scheduler_bounded_subreaper_prolific.h"
#include "quicksort.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

#define BOUND 4  /* maximum child processes spawned per batch */

static void child_work(uint8_t *shared_frames, int cols,
                       int row_start, int row_end)
{
    for (int r = row_start; r < row_end; r++) {
        uint8_t *row = shared_frames + (size_t)r * cols;
        if (cols > 1)
            quicksort_row(row, 0, cols - 1);
    }
}

double run_scheduler_bounded_subreaper_prolific(uint8_t *frames,
                                                int n_frames, int rows, int cols,
                                                int n_procs)
{
    int total_rows = n_frames * rows;
    size_t data_size = (size_t)total_rows * cols;

    /* Allocate shared memory region */
    uint8_t *shared = (uint8_t *)mmap(NULL, data_size,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) {
        perror("mmap");
        return -1.0;
    }
    memcpy(shared, frames, data_size);

#ifdef __linux__
    prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);
#endif

    int rows_per_proc = (total_rows + n_procs - 1) / n_procs;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    int row_cursor = 0;
    pid_t pids[BOUND];

    while (row_cursor < total_rows) {
        int batch = 0;

        while (batch < BOUND && row_cursor < total_rows) {
            int end = row_cursor + rows_per_proc;
            if (end > total_rows) end = total_rows;

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            if (pid == 0) {
                /* Child process */
                child_work(shared, cols, row_cursor, end);
                _exit(0);
            }
            pids[batch] = pid;
            row_cursor  = end;
            batch++;
        }

        /* Wait for all children in this batch */
        for (int i = 0; i < batch; i++)
            waitpid(pids[i], NULL, 0);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    /* Copy results back to caller's buffer */
    memcpy(frames, shared, data_size);
    munmap(shared, data_size);

    return (double)(t1.tv_sec - t0.tv_sec) +
           (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
}
