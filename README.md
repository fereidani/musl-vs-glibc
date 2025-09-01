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
- Date: 2025-08-30

Each row compares musl against the glibc baseline (lower ns/op is better).

| Benchmark          | glibc ns/op | musl ns/op | musl vs glibc       | Winner |
| ------------------ | ----------- | ---------- | ------------------- | ------ |
| atoi_parse         | 28.61       | 11.37      | +60.26% faster      | musl   |
| bsearch_int        | 32.86       | 45.58      | -38.71% slower      | glibc  |
| malloc_free_medium | 1385.33     | 4878.36    | -252.14% slower     | glibc  |
| malloc_free_small  | 27.90       | 181.35     | -550.00% slower     | glibc  |
| memcmp             | 0.48        | 0.41       | +14.58% faster      | musl   |
| memcpy             | 330.58      | 591.37     | -78.89% slower      | glibc  |
| memmove            | 236.06      | 256.59     | -8.70% slower       | glibc  |
| qsort_int          | 488953.09   | 1126757.27 | -130.44% slower     | glibc  |
| realloc_pattern    | 65.42       | 351.00     | -436.53% slower     | glibc  |
| regex_match        | 358.69      | 2295.62    | -540.00% slower     | glibc  |
| snprintf_mix       | 646.63      | 582.28     | +9.95% faster       | musl   |
| sprintf_float      | 580.67      | 291.16     | +49.86% faster      | musl   |
| sprintf_int        | 97.90       | 105.78     | -8.05% slower       | glibc  |
| strcat             | 15.30       | 31.32      | -104.71% slower     | glibc  |
| strchr             | 4.45        | 14.34      | -222.25% slower     | glibc  |
| strcmp             | 0.43        | 0.48       | -11.63% slower      | glibc  |
| strcpy             | 59.05       | 633.85     | -973.41% slower     | glibc  |
| strlen             | 0.38        | 0.38       | +0.00% (tie ≤0.50%) | tie    |
| strtod_parse       | 204.50      | 432.11     | -111.30% slower     | glibc  |
| strtok_parse       | 30.49       | 14.10      | +53.76% faster      | musl   |

### Summary

- Benchmarks compared: 20
- glibc faster (ns/op): 14
- musl faster (ns/op): 5
- Ties (ns/op): 1
- Overall (by count): glibc wins more benchmarks.
