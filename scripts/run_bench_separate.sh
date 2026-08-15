#!/usr/bin/env bash

# For a given benchmark and a given number system, run and compare with MPFR
# Unmodified commands to run wasm3

set -euo pipefail

build_wasm3() {
    local cflags=${1:-}
    local cxxflags=${2:-$cflags}

    mkdir -p "$BUILD_DIR"
    pushd "$BUILD_DIR" >/dev/null
    CC=clang CXX=clang++ cmake -S "$ROOT_DIR" -B . \
      -DCMAKE_C_FLAGS="${cflags}" \
      -DCMAKE_CXX_FLAGS="${cxxflags}" \
      -DBUILD_WASI=simple \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      2>/dev/null
    cmake --build . 2>/dev/null
    popd >/dev/null
}

cleanup_debug() {
    unset WASM3_DEBUG_LOG
    rm -f "$RESULT_DIR"/DEBUG-{ID,DD,EFT}.txt
}

if [[ $# -lt 3 ]]; then
    echo "Usage: $0 <input string for wasm3> <--dd|--eft|--eid> <output directory name>" >&2
    exit 1
fi

DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd -P)"
source "$DIR/../log.sh" # Important

input_str="$1"
flag="$2"
bench_out="$RESULT_DIR"/"$3" # Output directory for benchmark results

if [ -d "$bench_out" ]; then
    rm -rf "$bench_out"
fi
mkdir -p "$bench_out"

# Enter debug mode to check determinism (Compare correct DD and EFTSan)
build_wasm3 # no flag for correct DD mode
stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$RESULT_DIR"/TMP.txt || true

if ! "$SCRIPT_DIR"/gen_debug_ids.py "$RESULT_DIR"/TMP.txt > "$RESULT_DIR"/DEBUG-ID.txt 
then
    echo "No FP Operations" >&2
    rm "$RESULT_DIR"/TMP.txt
    exit 0
fi
rm "$RESULT_DIR"/TMP.txt

# Start to log operations to check determinism
export WASM3_DEBUG_LOG=1
# Run in correct DD mode
stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > /dev/null 2> "$RESULT_DIR"/DEBUG-DD.txt || true
# Run in EFTSan mode
build_wasm3 "-DWRONG_SUB -DWRONG_DIV"
stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > /dev/null 2> "$RESULT_DIR"/DEBUG-EFT.txt || true
# Check if the two debug files are the same
if ! "$SCRIPT_DIR"/samefile.sh "$RESULT_DIR"/DEBUG-DD.txt "$RESULT_DIR"/DEBUG-EFT.txt
then
    echo "Input program is NOT deterministic — Terminate" >&2
    cleanup_debug
    exit 1
fi

echo "Input program is deterministic — Continue"
cleanup_debug

# Get OP IDs of HREs and store in HRE-xxx.txt files
case "$flag" in
    --dd)
    # correct DD mode
        build_wasm3
        export WASM3_EOP_LOG=1 # Print OP IDs with HRE
        stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out"/HRE-DD.txt || true
        ;;
    --eft)
    # EFTSan mode
        build_wasm3 "-DWRONG_SUB -DWRONG_DIV"
        export WASM3_EOP_LOG=1 # Print OP IDs with HRE
        stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out"/HRE-EFT.txt || true
        ;;
    --eid)
    # PRO mode
        "$SCRIPT_DIR"/build-overridden-tab.sh "$input_str" "$bench_out"
        echo "Output saved in $bench_out"
        exit 0 # Skip the rest of the script
        ;;
    *)
        echo "Unknown flag: $flag" >&2
        exit 1
        ;;
esac

# MPFR mode
build_wasm3 "-DUSE_MPFR"
stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out"/HRE-MPFR.txt || true
unset WASM3_EOP_LOG

# Find OP IDs of FP/FN (Order of last two arguments is fixed for diff_ids.py)
case "$flag" in
    --dd)
        "$SCRIPT_DIR"/diff_ids.py "$bench_out" HRE-DD.txt HRE-MPFR.txt
        ;;
    --eft)
        "$SCRIPT_DIR"/diff_ids.py "$bench_out" HRE-EFT.txt HRE-MPFR.txt
        ;;
esac

# In MPFR mode
cp "$bench_out"/FN-ID.txt "$RESULT_DIR"/LOG-FN-ID.txt
cp "$bench_out"/FP-ID.txt "$RESULT_DIR"/LOG-FP-ID.txt
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out"/FP-MPFR-LOG.txt || true
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out"/FN-MPFR-LOG.txt || true
stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str \
  | tee  >(grep -E '^\(FN\)' >"$bench_out/FN-MPFR-LOG.txt") \
  | tee  >(grep -E '^\(FP\)' >"$bench_out/FP-MPFR-LOG.txt") \
  > /dev/null \
  || true

case $flag in
    --dd)
        build_wasm3
        # stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out"/FN-DD-LOG.txt || true
        # stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out"/FP-DD-LOG.txt || true
        stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str \
        | tee  >(grep -E '^\(FN\)' >"$bench_out/FN-DD-LOG.txt") \
        | tee  >(grep -E '^\(FP\)' >"$bench_out/FP-DD-LOG.txt") \
        > /dev/null \
        || true
        ;;
    --eft)
        build_wasm3 "-DWRONG_SUB -DWRONG_DIV"
        # stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out"/FN-EFT-LOG.txt || true
        # stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out"/FP-EFT-LOG.txt || true
        stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str \
        | tee  >(grep -E '^\(FN\)' >"$bench_out/FN-EFT-LOG.txt") \
        | tee  >(grep -E '^\(FP\)' >"$bench_out/FP-EFT-LOG.txt") \
        > /dev/null \
        || true
        ;;
esac

pushd $RESULT_DIR
rm LOG-FN-ID.txt
rm LOG-FP-ID.txt
popd

echo "Output saved in $bench_out"
