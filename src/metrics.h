#ifndef METRICS_H
#define METRICS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *scheduler_name;
    double T_serial;      /* Wall-clock time of sequential (1-thread) run (seconds) */
    double T_parallel;    /* Wall-clock time of parallel run (seconds) */
    int    N;             /* Number of threads/processes used */
    double speedup;       /* T_serial / T_parallel */
    double efficiency;    /* speedup / N */
    double performance;   /* 1.0 / T_parallel  (normalised throughput) */
    double sefficiency;   /* speedup / (N * log2(N)) — Scaled Efficiency */
} BenchmarkResult;

/**
 * Compute derived metrics (speedup, efficiency, performance, sefficiency)
 * from T_serial, T_parallel, and N stored in r.
 */
void compute_metrics(BenchmarkResult *r);

/**
 * Print a formatted table row for a single BenchmarkResult to stdout.
 */
void print_metrics(const BenchmarkResult *r);

/**
 * Write all results as a Markdown table to the specified file path.
 *
 * @param results  Array of BenchmarkResult.
 * @param count    Number of elements in the array.
 * @param path     File path to write (e.g. "results/benchmark_results.txt").
 */
void save_metrics_to_file(const BenchmarkResult *results, int count, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* METRICS_H */
