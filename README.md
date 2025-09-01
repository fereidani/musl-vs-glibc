# Benchmarking musl vs glibc

Compare musl and glibc using repeatable compile + benchmark runs driven by Zig's C compiler frontend.

## Prerequisites

- zig (provides zig cc). Install from https://ziglang.org/download/

Check zig:

```
zig version
```

## Running

```
./run.sh
```

On success this README is updated in-place with a timestamped results section.

## Benchmark Results: glibc vs musl

- Compiler: zig 0.15.1 using clang version 20.1.2
- Kernel: Linux 5.14.0-580.el9.x86_64 (#1 SMP PREEMPT_DYNAMIC Tue Apr 22 20:29:48 UTC 2025)
- CPU: Intel Xeon Processor (Skylake, IBRS, no TSX)
- Date: 2025-09-01

Each row compares musl against the glibc baseline (lower ns/op is better).

| Benchmark          | glibc ns/op | musl ns/op | musl vs glibc    | Winner |
| ------------------ | ----------- | ---------- | ---------------- | ------ |
| atoi_parse         | 25.54       | 7.96       | +68.83% faster   | musl   |
| bsearch_int        | 33.75       | 31.46      | +6.79% faster    | musl   |
| fgets_read         | 32.59       | 35.44      | -8.75% slower    | glibc  |
| file_io_rw         | 962077.76   | 1055768.67 | -9.74% slower    | glibc  |
| getline_read       | 31.99       | 65.23      | -103.91% slower  | glibc  |
| malloc_free_medium | 1483.58     | 5852.01    | -294.45% slower  | glibc  |
| malloc_free_small  | 27.93       | 259.46     | -828.97% slower  | glibc  |
| memcmp             | 0.40        | 0.49       | -22.50% slower   | glibc  |
| memcpy             | 343.33      | 781.69     | -127.68% slower  | glibc  |
| memmove            | 279.20      | 354.65     | -27.02% slower   | glibc  |
| qsort_int          | 537941.85   | 1615480.22 | -200.31% slower  | glibc  |
| realloc_pattern    | 41.73       | 232.65     | -457.51% slower  | glibc  |
| regex_match        | 294.19      | 2094.27    | -611.88% slower  | glibc  |
| snprintf_mix       | 520.49      | 550.52     | -5.77% slower    | glibc  |
| sprintf_float      | 571.27      | 286.10     | +49.92% faster   | musl   |
| sprintf_int        | 97.48       | 110.95     | -13.82% slower   | glibc  |
| strcat             | 14.45       | 31.43      | -117.51% slower  | glibc  |
| strchr             | 4.69        | 16.49      | -251.60% slower  | glibc  |
| strcmp             | 0.48        | 0.50       | -4.17% slower    | glibc  |
| strcpy             | 55.23       | 659.43     | -1093.97% slower | glibc  |
| strlen             | 0.51        | 0.50       | +1.96% faster    | musl   |
| strncat            | 16.17       | 26.30      | -62.65% slower   | glibc  |
| strncmp            | 25.83       | 1209.05    | -4580.80% slower | glibc  |
| strncpy            | 64.21       | 346.67     | -439.90% slower  | glibc  |
| strnlen            | 7.30        | 9.59       | -31.37% slower   | glibc  |
| strrchr            | 332.91      | 211.38     | +36.51% faster   | musl   |
| strstr_search      | 39.58       | 198.15     | -400.63% slower  | glibc  |
| strtod_parse       | 190.30      | 414.50     | -117.81% slower  | glibc  |
| strtok_parse       | 24.72       | 12.19      | +50.69% faster   | musl   |
| strtok_r_parse     | 24.87       | 10.24      | +58.83% faster   | musl   |
| vprintf_mix        | 1041.74     | 598.67     | +42.53% faster   | musl   |
| vsnprintf_mix      | 1077.73     | 610.88     | +43.32% faster   | musl   |

### Summary

- Benchmarks compared: 32
- glibc faster (ns/op): 23
- musl faster (ns/op): 9
- Ties (ns/op): 0
- Overall (by count): glibc wins more benchmarks.
