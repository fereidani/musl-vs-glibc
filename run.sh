#!/usr/bin/env bash
set -euo pipefail

SRC=benchmark.c
CC="zig cc"
CFLAGS="-O3 -march=native -mtune=native -flto=thin -fomit-frame-pointer -funroll-loops -DNDEBUG -fno-math-errno -fno-trapping-math"

if [[ ! -f $SRC ]]; then
    echo "Source file $SRC not found" >&2
    exit 1
fi

echo "Building (glibc)..."
$CC -target x86_64-linux-gnu  $SRC -o benchmark-gnu  $CFLAGS
echo "Building (musl)..."
$CC -target x86_64-linux-musl $SRC -o benchmark-musl $CFLAGS

mkdir -p results
gnu_csv=results/benchmark-gnu.csv
musl_csv=results/benchmark-musl.csv

# remove old files if they exist
rm -f "$gnu_csv" "$musl_csv"

echo "Running glibc binary..."
./benchmark-gnu > "$gnu_csv"
sleep 3
echo "Running musl binary..."
./benchmark-musl > "$musl_csv"

echo "Done. Files: $gnu_csv, $musl_csv"

python generate.py

# Cleanup
rm -f benchmark-gnu benchmark-musl