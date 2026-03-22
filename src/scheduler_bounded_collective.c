/*
 * scheduler_bounded_collective.c — Bounded Collective scheduler
 *
 * Strategy:
 *   - A thread pool of n_threads threads is created once.
 *   - Work (frame rows) is distributed via a shared task queue protected by
 *     a mutex/condition variable.
 *   - All threads participate collectively; bounded by the pool size.
 */

#include "scheduler_bounded_collective.h"
#include "quicksort.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ---- shared task queue ---- */
typedef struct {
    uint8_t *frames;
    int      cols;
    int      total_rows;
    int      next_row;      /* next row index to be claimed (under mutex) */
    int      done_rows;     /* rows fully processed (under mutex) */

    pthread_mutex_t mutex;
    pthread_cond_t  work_cond; /* workers wait here when queue is empty */
    pthread_cond_t  done_cond; /* main thread waits here for completion */
    int             shutdown;
} TaskQueue;

static void *pool_worker(void *arg)
{
    TaskQueue *q = (TaskQueue *)arg;

    while (1) {
        pthread_mutex_lock(&q->mutex);

        /* Wait until there is work or a shutdown signal */
        while (q->next_row >= q->total_rows && !q->shutdown)
            pthread_cond_wait(&q->work_cond, &q->mutex);

        if (q->next_row >= q->total_rows) {
            /* shutdown == 1 and no more work */
            pthread_mutex_unlock(&q->mutex);
            break;
        }

        /* Claim one row */
        int r = q->next_row++;
        pthread_mutex_unlock(&q->mutex);

        /* Process the row (outside the lock) */
        uint8_t *row = q->frames + (size_t)r * q->cols;
        if (q->cols > 1)
            quicksort_row(row, 0, q->cols - 1);

        /* Record completion */
        pthread_mutex_lock(&q->mutex);
        q->done_rows++;
        if (q->done_rows == q->total_rows)
            pthread_cond_signal(&q->done_cond);
        pthread_mutex_unlock(&q->mutex);
    }
    return NULL;
}

double run_scheduler_bounded_collective(uint8_t *frames,
                                        int n_frames, int rows, int cols,
                                        int n_threads)
{
    int total_rows = n_frames * rows;

    TaskQueue q;
    q.frames     = frames;
    q.cols       = cols;
    q.total_rows = total_rows;
    q.next_row   = 0;
    q.done_rows  = 0;
    q.shutdown   = 0;

    pthread_mutex_init(&q.mutex, NULL);
    pthread_cond_init(&q.work_cond, NULL);
    pthread_cond_init(&q.done_cond, NULL);

    pthread_t *tids = (pthread_t *)malloc((size_t)n_threads * sizeof(pthread_t));
    if (!tids) { perror("malloc"); exit(EXIT_FAILURE); }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    /* Start thread pool */
    for (int i = 0; i < n_threads; i++) {
        if (pthread_create(&tids[i], NULL, pool_worker, &q) != 0) {
            fprintf(stderr, "bounded_collective: pthread_create failed\n");
            exit(EXIT_FAILURE);
        }
    }

    /* Wake workers — work is already in the queue */
    pthread_mutex_lock(&q.mutex);
    pthread_cond_broadcast(&q.work_cond);
    pthread_mutex_unlock(&q.mutex);

    /* Wait until every row has been processed */
    pthread_mutex_lock(&q.mutex);
    while (q.done_rows < q.total_rows)
        pthread_cond_wait(&q.done_cond, &q.mutex);
    /* Signal shutdown so idle workers exit */
    q.shutdown = 1;
    pthread_cond_broadcast(&q.work_cond);
    pthread_mutex_unlock(&q.mutex);

    for (int i = 0; i < n_threads; i++)
        pthread_join(tids[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);

    free(tids);
    pthread_mutex_destroy(&q.mutex);
    pthread_cond_destroy(&q.work_cond);
    pthread_cond_destroy(&q.done_cond);

    return (double)(t1.tv_sec - t0.tv_sec) +
           (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
}
