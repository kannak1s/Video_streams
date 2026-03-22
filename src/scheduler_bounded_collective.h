/*
 * scheduler_bounded_collective.h — Bounded Collective scheduler header
 */

#ifndef SCHEDULER_BOUNDED_COLLECTIVE_H
#define SCHEDULER_BOUNDED_COLLECTIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

double run_scheduler_bounded_collective(uint8_t *frames,
                                        int n_frames, int rows, int cols,
                                        int n_threads);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_BOUNDED_COLLECTIVE_H */
