/*
 * scheduler_bounded_sko.c — Bounded Sko (Prolific + Collective) scheduler
 *
 * Strategy (two-level hierarchical):
 *   - Top level: prolific fork-join with bounded fanout B.
 *     Each "outer" thread is spawned by the main thread (up to B at a time).
 *   - Each outer thread creates a local sub-pool of (n_threads / B) worker
 *     threads that process its assigned rows collectively via a shared counter.
 *   - The outer thread joins all sub-pool workers before returning.
 */

#include "scheduler_bounded_sko.h"
#include "quicksort.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BOUND 4  /* outer fanout */

/* ---- inner (collective) sub-pool ---- */
typedef struct {
    uint8_t *frames;
    int      cols;
    int      row_start;
    int      row_end;
    int      next_row;      /* claimed rows within [row_start, row_end) */

    pthread_mutex_t mutex;
} SubPool;

static void *inner_worker(void *arg)
{
    SubPool *sp = (SubPool *)arg;

    while (1) {
        pthread_mutex_lock(&sp->mutex);
        if (sp->next_row >= sp->row_end) {
            pthread_mutex_unlock(&sp->mutex);
            break;
        }
        int r = sp->next_row++;
        pthread_mutex_unlock(&sp->mutex);

        uint8_t *row = sp->frames + (size_t)r * sp->cols;
        if (sp->cols > 1)
            quicksort_row(row, 0, sp->cols - 1);
    }
    return NULL;
}

/* ---- outer (prolific) thread ---- */
typedef struct {
    uint8_t *frames;
    int      cols;
    int      row_start;
    int      row_end;
    int      inner_threads;  /* size of inner sub-pool */
} OuterArgs;

static void *outer_worker(void *arg)
{
    OuterArgs *oa = (OuterArgs *)arg;
    int n_inner   = oa->inner_threads < 1 ? 1 : oa->inner_threads;

    SubPool sp;
    sp.frames    = oa->frames;
    sp.cols      = oa->cols;
    sp.row_start = oa->row_start;
    sp.row_end   = oa->row_end;
    sp.next_row  = oa->row_start;
    pthread_mutex_init(&sp.mutex, NULL);

    pthread_t *tids = (pthread_t *)malloc((size_t)n_inner * sizeof(pthread_t));
    if (!tids) { perror("malloc"); exit(EXIT_FAILURE); }

    for (int i = 0; i < n_inner; i++) {
        if (pthread_create(&tids[i], NULL, inner_worker, &sp) != 0) {
            fprintf(stderr, "sko: inner pthread_create failed\n");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < n_inner; i++)
        pthread_join(tids[i], NULL);

    free(tids);
    pthread_mutex_destroy(&sp.mutex);
    return NULL;
}

double run_scheduler_bounded_sko(uint8_t *frames,
                                 int n_frames, int rows, int cols,
                                 int n_threads)
{
    int total_rows   = n_frames * rows;
    int inner_size   = n_threads / BOUND;
    if (inner_size < 1) inner_size = 1;

    /* Rows assigned to each outer thread */
    int rows_per_outer = (total_rows + n_threads - 1) / n_threads;

    pthread_t  outer_tids[BOUND];
    OuterArgs  outer_args[BOUND];

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    int row_cursor = 0;

    while (row_cursor < total_rows) {
        int batch = 0;

        while (batch < BOUND && row_cursor < total_rows) {
            int end = row_cursor + rows_per_outer * inner_size;
            if (end > total_rows) end = total_rows;

            outer_args[batch].frames        = frames;
            outer_args[batch].cols          = cols;
            outer_args[batch].row_start     = row_cursor;
            outer_args[batch].row_end       = end;
            outer_args[batch].inner_threads = inner_size;

            if (pthread_create(&outer_tids[batch], NULL, outer_worker,
                               &outer_args[batch]) != 0) {
                fprintf(stderr, "sko: outer pthread_create failed\n");
                exit(EXIT_FAILURE);
            }

            row_cursor = end;
            batch++;
        }

        for (int i = 0; i < batch; i++)
            pthread_join(outer_tids[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    return (double)(t1.tv_sec - t0.tv_sec) +
           (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
}
