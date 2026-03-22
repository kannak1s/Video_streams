/*
 * scheduler_mmap_subreaper_collective.h
 */

#ifndef SCHEDULER_MMAP_SUBREAPER_COLLECTIVE_H
#define SCHEDULER_MMAP_SUBREAPER_COLLECTIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

double run_scheduler_mmap_subreaper_collective(uint8_t *frames,
                                               int n_frames, int rows, int cols,
                                               int n_procs);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_MMAP_SUBREAPER_COLLECTIVE_H */
