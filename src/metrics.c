#include "metrics.h"

#include <stdio.h>
#include <math.h>

void compute_metrics(BenchmarkResult *r)
{
    r->speedup     = (r->T_parallel > 0.0) ? r->T_serial / r->T_parallel : 0.0;
    r->efficiency  = (r->N > 0)            ? r->speedup / r->N           : 0.0;
    r->performance = (r->T_parallel > 0.0) ? 1.0 / r->T_parallel        : 0.0;

    if (r->N > 1)
        r->sefficiency = r->speedup / ((double)r->N * log2((double)r->N));
    else
        r->sefficiency = r->efficiency; /* log2(1) == 0; define SE = E for N=1 */
}

void print_metrics(const BenchmarkResult *r)
{
    printf("| %-46s | %3d | %12.6f | %14.6f | %9.4f | %10.4f | %11.4f | %11.4f |\n",
           r->scheduler_name,
           r->N,
           r->T_serial,
           r->T_parallel,
           r->speedup,
           r->efficiency,
           r->performance,
           r->sefficiency);
}

void save_metrics_to_file(const BenchmarkResult *results, int count, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "save_metrics_to_file: cannot open '%s' for writing\n", path);
        return;
    }

    /* Markdown table header */
    fprintf(fp, "# Scheduler Benchmark Results\n\n");
    fprintf(fp,
            "| %-46s | %3s | %12s | %14s | %9s | %10s | %11s | %11s |\n",
            "Scheduler", "N", "T_serial (s)", "T_parallel (s)",
            "Speedup", "Efficiency", "Performance", "SEfficiency");
    fprintf(fp,
            "|:------------------------------------------------|----:|-------------:|---------------:|----------:|-----------:|------------:|------------:|\n");

    for (int i = 0; i < count; i++) {
        fprintf(fp,
                "| %-46s | %3d | %12.6f | %14.6f | %9.4f | %10.4f | %11.4f | %11.4f |\n",
                results[i].scheduler_name,
                results[i].N,
                results[i].T_serial,
                results[i].T_parallel,
                results[i].speedup,
                results[i].efficiency,
                results[i].performance,
                results[i].sefficiency);
    }

    fclose(fp);
}
