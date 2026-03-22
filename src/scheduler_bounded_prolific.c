/*
 * scheduler_bounded_prolific.c — Bounded Prolific scheduler
 *
 * Strategy:
 *   - The total work (all rows of all frames) is divided into batches.
 *   - A parent thread spawns up to BOUND child threads for each batch.
 *   - The parent joins all children before spawning the next batch.
 *   - This models a prolific fork-join with bounded fanout B.
 */

#include "scheduler_bounded_prolific.h"
#include "quicksort.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BOUND 4  /* maximum number of child threads spawned at a time */

typedef struct {
    uint8_t *frames;
    int      n_frames;
    int      rows;
    int      cols;
    int      row_start; /* global start row (across all frames) */
    int      row_end;   /* global end row (exclusive) */
} WorkerArgs;

static void *worker(void *arg)
{
    WorkerArgs *wa = (WorkerArgs *)arg;
    int total_rows = wa->n_frames * wa->rows;
    int r_start    = wa->row_start;
    int r_end      = wa->row_end;
    if (r_end > total_rows) r_end = total_rows;

    for (int r = r_start; r < r_end; r++) {
        uint8_t *row = wa->frames + (size_t)r * wa->cols;
        if (wa->cols > 1)
            quicksort_row(row, 0, wa->cols - 1);
    }
    return NULL;
}

double run_scheduler_bounded_prolific(uint8_t *frames,
                                      int n_frames, int rows, int cols,
                                      int n_threads)
{
    int total_rows = n_frames * rows;

    /* Rows per logical thread */
    int rows_per_thread = (total_rows + n_threads - 1) / n_threads;

    /* We process BOUND threads at a time (bounded prolific) */
    pthread_t  tids[BOUND];
    WorkerArgs args[BOUND];

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    int row_cursor = 0;       /* next unassigned row */

    while (row_cursor < total_rows) {
        int batch = 0;

        /* Spawn up to BOUND threads */
        while (batch < BOUND && row_cursor < total_rows) {
            int end = row_cursor + rows_per_thread;
            if (end > total_rows) end = total_rows;

            args[batch].frames    = frames;
            args[batch].n_frames  = n_frames;
            args[batch].rows      = rows;
            args[batch].cols      = cols;
            args[batch].row_start = row_cursor;
            args[batch].row_end   = end;

            if (pthread_create(&tids[batch], NULL, worker, &args[batch]) != 0) {
                fprintf(stderr, "bounded_prolific: pthread_create failed\n");
                exit(EXIT_FAILURE);
            }

            row_cursor = end;
            batch++;
        }

        /* Join all threads in this batch */
        for (int i = 0; i < batch; i++)
            pthread_join(tids[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    return (double)(t1.tv_sec - t0.tv_sec) +
           (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
}
