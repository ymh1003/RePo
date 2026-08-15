# export ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd -P)"
export BUILD_DIR="$ROOT_DIR/build"
export SCRIPT_DIR="$ROOT_DIR/scripts"
export RESULT_DIR="$ROOT_DIR/report"
# export WASM3_LOAD_LOG=1
# export WASM3_STORE_LOG=1
# export WASM3_EOP_LOG=1
# export WASM3_DEBUG_LOG=1
# export WASM3_FULL_LOG=1
unset WASM3_LOAD_LOG
unset WASM3_STORE_LOG
unset WASM3_EOP_LOG
unset WASM3_DEBUG_LOG
unset WASM3_FULL_LOG