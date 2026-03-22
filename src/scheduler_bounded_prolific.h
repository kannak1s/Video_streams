/*
 * scheduler_bounded_prolific.h — Bounded Prolific scheduler header
 *
 * Each "parent" thread spawns up to B (= BOUND) child threads using
 * pthread_create.  Children process assigned frame-row ranges; the parent
 * joins all children before spawning the next batch.
 *
 * Public interface (extern "C" compatible):
 *   double run_scheduler_bounded_prolific(
 *           uint8_t *frames, int n_frames, int rows, int cols, int n_threads);
 */

#ifndef SCHEDULER_BOUNDED_PROLIFIC_H
#define SCHEDULER_BOUNDED_PROLIFIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run the Bounded Prolific scheduler over a flat frame batch.
 *
 * @param frames    Flat array: n_frames * rows * cols bytes.
 * @param n_frames  Number of frames in the batch.
 * @param rows      Rows per frame.
 * @param cols      Columns per frame (bytes per row).
 * @param n_threads Total number of worker threads to utilise.
 * @return          Wall-clock execution time in seconds.
 */
double run_scheduler_bounded_prolific(uint8_t *frames,
                                      int n_frames, int rows, int cols,
                                      int n_threads);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_BOUNDED_PROLIFIC_H */
