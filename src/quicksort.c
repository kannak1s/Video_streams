#include "quicksort.h"

/* Swap two bytes in place. */
static inline void swap_bytes(uint8_t *a, uint8_t *b)
{
    uint8_t tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * Median-of-three: choose pivot from arr[left], arr[mid], arr[right] and
 * place it at arr[right-1].  Returns the pivot value.
 */
static uint8_t median_of_three(uint8_t *arr, int left, int right)
{
    int mid = left + (right - left) / 2;

    /* Sort left, mid, right into order */
    if (arr[left] > arr[mid])
        swap_bytes(&arr[left], &arr[mid]);
    if (arr[left] > arr[right])
        swap_bytes(&arr[left], &arr[right]);
    if (arr[mid] > arr[right])
        swap_bytes(&arr[mid], &arr[right]);

    /* Place pivot (median) at right-1 */
    swap_bytes(&arr[mid], &arr[right - 1]);
    return arr[right - 1];
}

void quicksort_row(uint8_t *arr, int left, int right)
{
    /* Base cases: 0 or 1 element */
    if (left >= right)
        return;

    /* Two elements: just swap if needed */
    if (right - left == 1) {
        if (arr[left] > arr[right])
            swap_bytes(&arr[left], &arr[right]);
        return;
    }

    /* Choose pivot via median-of-three */
    uint8_t pivot = median_of_three(arr, left, right);

    /* Partition around pivot (pivot is at right-1) */
    int i = left;
    int j = right - 1;

    while (1) {
        while (arr[++i] < pivot)
            ;
        while (arr[--j] > pivot)
            ;
        if (i >= j)
            break;
        swap_bytes(&arr[i], &arr[j]);
    }

    /* Restore pivot to its final position */
    swap_bytes(&arr[i], &arr[right - 1]);

    /* Recurse on sub-arrays (excluding pivot) */
    quicksort_row(arr, left, i - 1);
    quicksort_row(arr, i + 1, right);
}
