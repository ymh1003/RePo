#!/bin/bash

set -e

LF="$ROOT_DIR/report/time.log"
TIME_PREFIX="${TIME_PREFIX:-}"
ERR_LOG=""

cleanup() {
    if [[ -n "$ERR_LOG" && -f "$ERR_LOG" ]]; then
        rm -f "$ERR_LOG"
    fi
}

trap cleanup EXIT

if [[ -z "$TIME_PREFIX" ]]; then
    if [[ -x /usr/bin/time ]]; then
        TIME_PREFIX="/usr/bin/time -p"
    else
        TIME_PREFIX="time -p"
    fi
fi

echo "Benchmarking $1:" >>"$LF" # Benchmark directory
ERR_LOG="$(mktemp "${TMPDIR:-/tmp}/time_make.XXXXXX")"
make -k SCRIPT="$TIME_PREFIX $ROOT_DIR/scripts/run_bench_together.sh" \
     -C "$@" 2>"$ERR_LOG" \
     || true
cat "$ERR_LOG" | tee -a "$LF" >&2
