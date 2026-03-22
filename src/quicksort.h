#ifndef QUICKSORT_H
#define QUICKSORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * In-place QuickSort on a uint8_t pixel row using median-of-three pivot.
 * Handles edge cases: single-element rows and rows of length 0.
 *
 * @param arr   Pointer to the pixel array (row).
 * @param left  Left index (inclusive).
 * @param right Right index (inclusive).
 */
void quicksort_row(uint8_t *arr, int left, int right);

#ifdef __cplusplus
}
#endif

#endif /* QUICKSORT_H */
