set -e -x

export ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd -P)"
export EXE_LIMIT=20 # Limit the number of re-evaluations
export SECOND_ORDER_ERR=1 # Enable second order error for multiplication
# unset SECOND_ORDER_ERR # Disable second order error for multiplication
export MPFR_PREC=512
export COND_THRESH=47

export ENABLE_FULL_LOG=1 # Enable full logging to stderr
unset LOG_LIMIT
# unset ENABLE_FULL_LOG # Disable full logging to stderr
export ENABLE_FPN_LOG=0

time_script="$ROOT_DIR/time_make.sh"

# To run a single benchmark:
#     $time_script [...] [dirname/benchmark]

# EFTSan microbenchmarks
$time_script "$ROOT_DIR/eftSan_bench/microbench/" microbench/diff-roots-simple

# NAS Parallel Benchmarks (NPB)
# $time_script "$ROOT_DIR/npb-wasm" all

# Polybench
# $time_script "$ROOT_DIR/polybench-wasm" all

# Rodinia
# $time_script "$ROOT_DIR/rodinia-wasm" all

# FPBench 
# $time_script "$ROOT_DIR/fpbench-wasm" all

# Create HTML page
./create-html.sh