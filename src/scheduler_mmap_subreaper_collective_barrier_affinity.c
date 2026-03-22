/*
 * scheduler_mmap_subreaper_collective_barrier_affinity.c
 *
 * Extends the mmap collective subreaper scheduler with:
 *   - CPU affinity: each worker process is pinned to a specific core via
 *     sched_setaffinity() (Linux only).
 *   - Group barrier: a POSIX barrier (pthread_barrier_t in
 *     PTHREAD_PROCESS_SHARED mode) placed in the mmap region ensures all
 *     workers synchronize before and after the processing phase.
 *
 * Shared mmap region layout:
 *   [0 .. data_size)                  — frame pixel data
 *   [data_size]                        — volatile int: shared row counter
 *   [data_size + sizeof(int) (aligned)] — pthread_barrier_t
 */

#define _GNU_SOURCE
#include "scheduler_mmap_subreaper_collective_barrier_affinity.h"
#include "quicksort.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

#ifdef __linux__
#include <sys/prctl.h>
#include <sched.h>
#endif

/* Align offset up to the alignment of pthread_barrier_t */
static size_t align_up(size_t offset, size_t align)
{
    return (offset + align - 1) & ~(align - 1);
}

double run_scheduler_mmap_subreaper_collective_barrier_affinity(
        uint8_t *frames, int n_frames, int rows, int cols, int n_procs)
{
    int    total_rows  = n_frames * rows;
    size_t data_size   = (size_t)total_rows * cols;

    /* Calculate offsets inside the shared region */
    size_t counter_off = data_size;
    size_t barrier_off = align_up(counter_off + sizeof(int),
                                  __alignof__(pthread_barrier_t));
    size_t region_size = barrier_off + sizeof(pthread_barrier_t);

    uint8_t *region = (uint8_t *)mmap(NULL, region_size,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        return -1.0;
    }

    /* Initialise shared data */
    memcpy(region, frames, data_size);
    volatile int *counter = (volatile int *)(region + counter_off);
    *counter = 0;

    /* Initialise a process-shared barrier */
    pthread_barrier_t *barrier =
        (pthread_barrier_t *)(region + barrier_off);
    pthread_barrierattr_t battr;
    pthread_barrierattr_init(&battr);
    pthread_barrierattr_setpshared(&battr, PTHREAD_PROCESS_SHARED);
    /* n_procs workers + 1 parent all wait at the start barrier */
    pthread_barrier_init(barrier, &battr, (unsigned)(n_procs + 1));
    pthread_barrierattr_destroy(&battr);

#ifdef __linux__
    prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);
#endif

    pid_t *pids = (pid_t *)malloc((size_t)n_procs * sizeof(pid_t));
    if (!pids) { perror("malloc"); exit(EXIT_FAILURE); }

    int n_cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);

    for (int i = 0; i < n_procs; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(EXIT_FAILURE); }
        if (pid == 0) {
            /* ---- Child process ---- */
#ifdef __linux__
            /* Pin to a specific CPU (round-robin) */
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i % n_cpus, &cpuset);
            sched_setaffinity(0, sizeof(cpuset), &cpuset);
#endif
            /* Wait at start barrier (all workers + parent) */
            pthread_barrier_wait(barrier);

            /* Self-schedule rows via atomic counter */
            while (1) {
                int r = __sync_fetch_and_add(counter, 1);
                if (r >= total_rows) break;
                uint8_t *row = region + (size_t)r * cols;
                if (cols > 1)
                    quicksort_row(row, 0, cols - 1);
            }

            /* Wait at end barrier */
            pthread_barrier_wait(barrier);
            _exit(0);
        }
        pids[i] = pid;
    }

    /* Parent waits at start barrier, then measures only the computation */
    pthread_barrier_wait(barrier);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    /* Wait at end barrier — all workers have finished */
    pthread_barrier_wait(barrier);

    clock_gettime(CLOCK_MONOTONIC, &t1);

    for (int i = 0; i < n_procs; i++)
        waitpid(pids[i], NULL, 0);

    memcpy(frames, region, data_size);
    free(pids);

    pthread_barrier_destroy(barrier);
    munmap(region, region_size);

    return (double)(t1.tv_sec - t0.tv_sec) +
           (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
}
