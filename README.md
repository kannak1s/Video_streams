# Video_streams

A parallel video frame processing system in C/C++ that:

1. **T1** — Loads video frames using OpenCV, applies an in-place **QuickSort-based per-row pixel sorting** to each frame, and writes the resulting row-sorted video to disk.
2. **T2** — Benchmarks **6 thread/process scheduling strategies**, computing **Performance**, **Efficiency**, **Speedup**, and **SEfficiency** for each.

---

## Repository Structure

```
Video_streams/
├── Makefile
├── README.md
├── src/
│   ├── quicksort.h / quicksort.c                       ← T1: QuickSort on pixel rows
│   ├── frame_processor.h / frame_processor.cpp         ← T1: OpenCV frame loading & sorting
│   ├── metrics.h / metrics.c                           ← Performance metrics
│   ├── scheduler_bounded_prolific.h/c                  ← Scheduler 1
│   ├── scheduler_bounded_collective.h/c                ← Scheduler 2
│   ├── scheduler_bounded_sko.h/c                       ← Scheduler 3
│   ├── scheduler_bounded_subreaper_prolific.h/c        ← Scheduler 4
│   ├── scheduler_mmap_subreaper_collective.h/c         ← Scheduler 5
│   ├── scheduler_mmap_subreaper_collective_barrier_affinity.h/c ← Scheduler 6
│   └── main.cpp                                        ← Entry point
└── results/
    └── benchmark_results.txt                           ← Written at runtime
```

---

## Dependencies

| Dependency | Notes |
|---|---|
| `gcc` / `g++` | C17 / C++17 |
| OpenCV 4.x | `libopencv-dev` (Debian/Ubuntu) |
| `pthreads` | POSIX threads (part of glibc) |
| `librt` | POSIX realtime extensions (`clock_gettime`) |
| `mmap` / `prctl` | Linux POSIX shared memory and subreaper API |

Install OpenCV on Debian/Ubuntu:

```bash
sudo apt-get install libopencv-dev pkg-config
```

---

## Build

```bash
make
```

This creates the `results/` directory (if absent) and compiles `video_sort`.

```bash
make clean   # remove object files and binary
```

---

## Run

### Full run (T1 + T2)

```bash
./video_sort input.mp4
```

- T1 processes every frame: converts to grayscale, sorts each pixel row with QuickSort, writes `input_sorted.avi`.
- T2 runs all 6 scheduler benchmarks on a synthetic 100-frame × 480 × 640 batch and saves results.

### Skip T1 (T2 only)

```bash
./video_sort --skip-t1
```

Runs only the scheduler benchmarks using a synthetic frame batch (no video file required).

### Custom thread count

```bash
./video_sort input.mp4 --threads 8
```

---

## T1 — Video Frame Row Sorting

`src/quicksort.c` implements an **in-place QuickSort** on a `uint8_t*` array (pixel row) using the **median-of-three** pivot strategy. Edge cases (0-length or 1-element rows) are handled explicitly.

`src/frame_processor.cpp` uses `cv::VideoCapture` to decode each frame, converts it to grayscale (`COLOR_BGR2GRAY`), sorts every row by calling `quicksort_row()` on `frame.ptr<uint8_t>(r)`, and writes the result to `<input_basename>_sorted.avi` (XVID codec). Per-frame timing in microseconds is printed to stdout.

---

## T2 — Scheduler Benchmarks

All schedulers apply `quicksort_row()` to every row of a 100-frame × 480 × 640 synthetic batch. A sequential baseline is measured first; parallel execution time is then measured for each scheduler. Metrics are computed and a Markdown table is saved to `results/benchmark_results.txt`.

### Scheduler Strategies

| # | Name | Strategy |
|---|---|---|
| 1 | **Bounded Prolific** | Main thread spawns batches of up to `B=4` `pthread` worker threads. Each batch is joined before spawning the next (fork-join, bounded fanout). |
| 2 | **Bounded Collective** | A pool of `N` threads is created once. Work (rows) is distributed via a mutex-protected shared counter. Workers self-schedule by claiming rows atomically under a mutex. |
| 3 | **Bounded Sko** | Two-level: `B` outer threads (prolific) each manage a local collective sub-pool of `N/B` worker threads for their assigned row range. |
| 4 | **Bounded Subreaper Prolific** | Uses `fork()` (processes). Parent calls `prctl(PR_SET_CHILD_SUBREAPER)`. Frame data is held in `MAP_SHARED|MAP_ANONYMOUS` mmap memory. Workers process fixed row ranges; parent `waitpid()`s for all. |
| 5 | **Mmap Bounded Subreaper Collective** | Same subreaper setup. Workers self-schedule by atomically incrementing a shared mmap counter (`__sync_fetch_and_add`). No fixed work assignment. |
| 6 | **Mmap Bounded Subreaper Collective + Barrier + Affinity** | Extends scheduler 5 with: CPU affinity (`sched_setaffinity`) per worker, and a `PTHREAD_PROCESS_SHARED` barrier in the mmap region to synchronize all workers before/after the computation phase. The parent measures only the synchronized computation window. |

### Metrics

| Metric | Formula |
|---|---|
| Speedup | `T_serial / T_parallel` |
| Efficiency | `Speedup / N` |
| Performance | `1 / T_parallel` |
| SEfficiency | `Speedup / (N × log₂N)` |

---

## Sample Output

```
=== T2: Scheduler benchmarks ===
Threads/processes: 4

| Scheduler                                      |   N | T_serial (s) | T_parallel (s) |   Speedup | Efficiency | Performance | SEfficiency |
|:------------------------------------------------|----:|-------------:|---------------:|----------:|-----------:|------------:|------------:|
| Bounded Prolific                               |   4 |     0.053200 |       0.016450 |    3.2341 |     0.8085 |      60.794 |      0.4043 |
| Bounded Collective                             |   4 |     0.053200 |       0.015820 |    3.3628 |     0.8407 |      63.211 |      0.4204 |
| Bounded Sko (Prolific + Collective)            |   4 |     0.053200 |       0.016100 |    3.3043 |     0.8261 |      62.112 |      0.4130 |
| Bounded Subreaper Prolific                     |   4 |     0.053200 |       0.018300 |    2.9071 |     0.7268 |      54.645 |      0.3634 |
| Mmap Bounded Subreaper Collective              |   4 |     0.053200 |       0.017900 |    2.9721 |     0.7430 |      55.866 |      0.3715 |
| Mmap Bounded Subreaper Collective + Barrier + Affinity |   4 |     0.053200 |       0.014500 |    3.6690 |     0.9172 |      68.966 |      0.4586 |

Results saved to results/benchmark_results.txt
```

---

## Notes

- Schedulers 4–6 (process-based) default to `min(N_threads, 8)` processes to avoid excessive forking.
- `prctl` and `sched_setaffinity` are guarded with `#ifdef __linux__` for portability.
- Shared memory regions are properly `munmap`'d; all child processes are `waitpid`'d.
- The `results/` directory is created automatically at build time (`mkdir -p results`).
