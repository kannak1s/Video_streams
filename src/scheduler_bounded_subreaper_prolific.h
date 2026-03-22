/*
 * scheduler_bounded_subreaper_prolific.h
 */

#ifndef SCHEDULER_BOUNDED_SUBREAPER_PROLIFIC_H
#define SCHEDULER_BOUNDED_SUBREAPER_PROLIFIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

double run_scheduler_bounded_subreaper_prolific(uint8_t *frames,
                                                int n_frames, int rows, int cols,
                                                int n_procs);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_BOUNDED_SUBREAPER_PROLIFIC_H */
