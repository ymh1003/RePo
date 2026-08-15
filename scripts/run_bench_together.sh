#!/usr/bin/env bash

# For a given benchmark, run with all available number systems, and compare with MPFR
# Store results in sub-directories named by the corresponding number system

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

record_fp_cycles() {
    local label=$1

    echo "${label}:" >> "$bench_out_time"
    { WASM3_FP_TIMING=1 stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > /dev/null; } \
      2>> "$bench_out_time" || true
    printf '\n' >> "$bench_out_time"
}

cleanup_debug() {
    unset WASM3_DEBUG_LOG
    rm -f "$RESULT_DIR"/DEBUG-{ID,DD,EFT}.txt
}

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <input string for wasm3> <output directory name>" >&2
    exit 1
fi

DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd -P)"
source "$DIR/../log.sh" # Important

input_str="$1"
bench_out="$RESULT_DIR"/"$2" # Output directory for benchmark results

if [ -d "$bench_out" ]; then
    rm -rf "$bench_out"
fi
mkdir -p "$bench_out"

# # Enter debug mode to check determinism (Compare correct DD and EFTSan)
# build_wasm3 "-DUSE_DD" # no flag for Correct DD mode
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$RESULT_DIR"/TMP.txt || true

# if ! "$SCRIPT_DIR"/gen_debug_ids.py "$RESULT_DIR"/TMP.txt > "$RESULT_DIR"/DEBUG-ID.txt 
# then
#     echo "No FP Operations" >&2
#     rm "$RESULT_DIR"/TMP.txt
#     exit 0
# fi
# rm "$RESULT_DIR"/TMP.txt

# # Start to log operations to check determinism
# export WASM3_DEBUG_LOG=1
# # Run in Correct DD mode
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > /dev/null 2> "$RESULT_DIR"/DEBUG-DD.txt || true
# # Run in EFTSan mode
# build_wasm3 "-DUSE_EFTSAN"
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > /dev/null 2> "$RESULT_DIR"/DEBUG-EFT.txt || true
# # Check if the two debug files are the same
# if ! "$SCRIPT_DIR"/samefile.sh "$RESULT_DIR"/DEBUG-DD.txt "$RESULT_DIR"/DEBUG-EFT.txt
# then
#     echo "Input program is NOT deterministic — Terminate" >&2
#     cleanup_debug
#     exit 1
# fi

# echo "Input program is deterministic — Continue"
# cleanup_debug

# Get OP IDs of HREs and store in HRE-xxx.txt files
bench_out_dd="$bench_out"/dd # EFTSan-Fixed
mkdir "$bench_out_dd"

# bench_out_eft="$bench_out"/eft # EFTSan-Buggy
# mkdir "$bench_out_eft"

# bench_out_mpfr_dd="$bench_out"/mpfr-dd # MPFR-DD
# mkdir "$bench_out_mpfr_dd"

bench_out_qd="$bench_out"/qd # QD
mkdir "$bench_out_qd"

bench_out_time="$bench_out"/TIME.txt
: > "$bench_out_time"

# PRO mode
"$SCRIPT_DIR"/build-overridden-tab.sh "$input_str" "$bench_out"

# Uninstrumented mode (pure wasm3)
build_wasm3 ""
stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > /dev/null || true

record_fp_cycles "Uninstrumented"

# Correct DD mode
build_wasm3 "-DUSE_DD"
export WASM3_EOP_LOG=1 # Print OP IDs with HRE
stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out_dd"/HRE-DD.txt || true
unset WASM3_EOP_LOG

record_fp_cycles "DD"

# # EFTSan mode
# build_wasm3 "-DUSE_EFTSAN"
# export WASM3_EOP_LOG=1 # Print OP IDs with HRE
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out_eft"/HRE-EFT.txt || true
# unset WASM3_EOP_LOG

# echo "EFTSan:" >> "$bench_out_time"
# { time stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > /dev/null; } \
#   2>> "$bench_out_time" || true
# printf '\n' >> "$bench_out_time"

# QD mode
build_wasm3 "-DUSE_QD" "-DUSE_QD"
export WASM3_EOP_LOG=1 # Print OP IDs with HRE
stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out_qd"/HRE-QD.txt || true
unset WASM3_EOP_LOG

record_fp_cycles "QD"

# # MPFR_DD mode
# build_wasm3 "-DUSE_MPFR_DD"
# export WASM3_EOP_LOG=1 # Print OP IDs with HRE
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str > "$bench_out_mpfr_dd"/HRE-MPFR-DD.txt || true

# Move HRE-MPFR.txt (generated from running PRO mode) to individual sub-directories
cp "$bench_out"/HRE-MPFR.txt "$bench_out_dd"
# cp "$bench_out"/HRE-MPFR.txt "$bench_out_eft"
# cp "$bench_out"/HRE-MPFR.txt "$bench_out_mpfr_dd"
cp "$bench_out"/HRE-MPFR.txt "$bench_out_qd"
rm "$bench_out"/HRE-MPFR.txt

# Find OP IDs of FP/FN (Order of last two arguments is fixed for diff_ids.py)
"$SCRIPT_DIR"/diff_ids.py "$bench_out_dd" HRE-DD.txt HRE-MPFR.txt
# "$SCRIPT_DIR"/diff_ids.py "$bench_out_eft" HRE-EFT.txt HRE-MPFR.txt
# "$SCRIPT_DIR"/diff_ids.py "$bench_out_mpfr_dd" HRE-MPFR-DD.txt HRE-MPFR.txt
"$SCRIPT_DIR"/diff_ids.py "$bench_out_qd" HRE-QD.txt HRE-MPFR.txt

# # Get detailed FP/FN logs in MPFR mode
# cp "$bench_out_dd"/FN-ID.txt "$RESULT_DIR"/LOG-FN-ID.txt
# cp "$bench_out_dd"/FP-ID.txt "$RESULT_DIR"/LOG-FP-ID.txt
# build_wasm3 "-DUSE_MPFR"
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str \
#   | tee  >(grep -E '^\(FN\)' >"$bench_out_dd/FN-MPFR-LOG.txt") \
#   | tee  >(grep -E '^\(FP\)' >"$bench_out_dd/FP-MPFR-LOG.txt") \
#   > /dev/null \
#   || true

# cp "$bench_out_eft"/FN-ID.txt "$RESULT_DIR"/LOG-FN-ID.txt
# cp "$bench_out_eft"/FP-ID.txt "$RESULT_DIR"/LOG-FP-ID.txt
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str \
#   | tee  >(grep -E '^\(FN\)' >"$bench_out_eft/FN-MPFR-LOG.txt") \
#   | tee  >(grep -E '^\(FP\)' >"$bench_out_eft/FP-MPFR-LOG.txt") \
#   > /dev/null \
#   || true

# # Get detailed FP/FN logs in EFTSan mode
# build_wasm3 "-DUSE_EFTSAN"
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str \
#   | tee  >(grep -E '^\(FN\)' >"$bench_out_eft/FN-EFT-LOG.txt") \
#   | tee  >(grep -E '^\(FP\)' >"$bench_out_eft/FP-EFT-LOG.txt") \
#   > /dev/null \
#   || true

# # Get detailed FP/FN logs in Correct DD mode
# # cp "$bench_out_dd"/FN-ID.txt "$RESULT_DIR"/LOG-FN-ID.txt
# # cp "$bench_out_dd"/FP-ID.txt "$RESULT_DIR"/LOG-FP-ID.txt
# build_wasm3 "-DUSE_DD"
# cp "$bench_out_dd"/FN-ID.txt "$RESULT_DIR"/LOG-FN-ID.txt
# cp "$bench_out_dd"/FP-ID.txt "$RESULT_DIR"/LOG-FP-ID.txt
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str \
#   | tee  >(grep -E '^\(FN\)' >"$bench_out_dd/FN-DD-LOG.txt") \
#   | tee  >(grep -E '^\(FP\)' >"$bench_out_dd/FP-DD-LOG.txt") \
#   > /dev/null \
#   || true

# # Get detailed FP/FN logs in MPFR_DD mode
# build_wasm3 "-DUSE_MPFR_DD"
# cp "$bench_out_mpfr_dd"/FN-ID.txt "$RESULT_DIR"/LOG-FN-ID.txt
# cp "$bench_out_mpfr_dd"/FP-ID.txt "$RESULT_DIR"/LOG-FP-ID.txt
# stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str \
#   | tee  >(grep -E '^\(FN\)' >"$bench_out_mpfr_dd/FN-MPFR-DD-LOG.txt") \
#   | tee  >(grep -E '^\(FP\)' >"$bench_out_mpfr_dd/FP-MPFR-DD-LOG.txt") \
#   > /dev/null \
#   || true

# Cleanup
pushd $RESULT_DIR
rm -f LOG-FN-ID.txt LOG-FP-ID.txt
popd

echo "Output saved in $bench_out"
