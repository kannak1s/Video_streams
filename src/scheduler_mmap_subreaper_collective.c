/*
 * scheduler_mmap_subreaper_collective.c — Mmap Bounded Subreaper Collective
 *
 * Strategy:
 *   - Same subreaper setup as scheduler 4 (fork + prctl).
 *   - Workers share a collective task counter in the mmap'd region.
 *   - Each worker atomically claims the next unprocessed frame-row batch via
 *     __sync_fetch_and_add on the shared counter.
 *   - No fixed work assignment — collective self-scheduling via shared mmap.
 */

#include "scheduler_mmap_subreaper_collective.h"
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

/* Layout of the shared mmap region:
 *   [0 .. data_size)           — frame pixel data (uint8_t)
 *   [data_size]                — volatile int: shared row counter
 */

static void child_collective(uint8_t *shared_frames,
                              volatile int *counter,
                              int total_rows, int cols)
{
    while (1) {
        int r = __sync_fetch_and_add(counter, 1);
        if (r >= total_rows)
            break;
        uint8_t *row = shared_frames + (size_t)r * cols;
        if (cols > 1)
            quicksort_row(row, 0, cols - 1);
    }
}

double run_scheduler_mmap_subreaper_collective(uint8_t *frames,
                                               int n_frames, int rows, int cols,
                                               int n_procs)
{
    int    total_rows = n_frames * rows;
    size_t data_size  = (size_t)total_rows * cols;
    size_t region_size = data_size + sizeof(int);

    uint8_t *region = (uint8_t *)mmap(NULL, region_size,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        return -1.0;
    }

    memcpy(region, frames, data_size);
    volatile int *counter = (volatile int *)(region + data_size);
    *counter = 0;

#ifdef __linux__
    prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);
#endif

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    pid_t *pids = (pid_t *)malloc((size_t)n_procs * sizeof(pid_t));
    if (!pids) { perror("malloc"); exit(EXIT_FAILURE); }

    for (int i = 0; i < n_procs; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(EXIT_FAILURE); }
        if (pid == 0) {
            child_collective((uint8_t *)region, counter, total_rows, cols);
            _exit(0);
        }
        pids[i] = pid;
    }

    for (int i = 0; i < n_procs; i++)
        waitpid(pids[i], NULL, 0);

    clock_gettime(CLOCK_MONOTONIC, &t1);

    memcpy(frames, region, data_size);
    free(pids);
    munmap(region, region_size);

    return (double)(t1.tv_sec - t0.tv_sec) +
           (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
}
