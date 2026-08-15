#!/usr/bin/env bash

set -euo pipefail

cleanup_on_error() {
    set +e
    printf './build-overridden-tab.sh failed — cleaning up stage\n' >&2
    pushd "$RESULT_DIR"
    rm * # only remove shared metadata files
    popd

    exit 1
}

trap cleanup_on_error ERR

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
        2>>"$bench_out_time" || true
    printf '\n' >> "$bench_out_time"
}

abort_if_sigabrt() {
    local rc=$1
    if (( rc == 134 )); then # 128 + 6  (6 = SIGABRT)
        echo "fatal: aborted (SIGABRT) — stopping script" >&2
        cleanup_on_error
        exit 134
    fi
}

save_orig_results() {
    pushd "$RESULT_DIR"
    cp SILENT-ID.txt SILENT-ID-ORIG.txt
    cp PROBE-ID.txt PROBE-ID-ORIG.txt
    cp TEMP_OVERRIDE.bin TEMP_OVERRIDE-ORIG.bin
    cp OVERRIDE.bin OVERRIDE-ORIG.bin
    popd
}

recover_orig_results() {
    pushd "$RESULT_DIR"
    cp SILENT-ID-ORIG.txt SILENT-ID.txt
    cp PROBE-ID-ORIG.txt PROBE-ID.txt
    cp TEMP_OVERRIDE-ORIG.bin TEMP_OVERRIDE.bin
    cp OVERRIDE-ORIG.bin OVERRIDE.bin
    popd
}

save_new_results() {
    pushd "$RESULT_DIR"
    cp SILENT-ID.txt SILENT-ID-NEW.txt
    cp PROBE-ID.txt PROBE-ID-NEW.txt
    cp TEMP_OVERRIDE.bin TEMP_OVERRIDE-NEW.bin
    cp OVERRIDE.bin OVERRIDE-NEW.bin
    popd
}

recover_new_results() {
    pushd "$RESULT_DIR"
    cp SILENT-ID-NEW.txt SILENT-ID.txt
    cp PROBE-ID-NEW.txt PROBE-ID.txt
    cp TEMP_OVERRIDE-NEW.bin TEMP_OVERRIDE.bin
    cp OVERRIDE-NEW.bin OVERRIDE.bin
    popd
}

# Start
if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <input string for wasm3> <result directory filepath>" >&2
    exit 2
fi

DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd -P)"
source "$DIR/../log.sh" # Important

input_str="$1"
bench_out="$2"
bench_out_eid="$bench_out"/eid

bench_out_time="$bench_out"/TIME.txt

if [ -d "$bench_out_eid" ]; then
    rm -r "$bench_out_eid"
fi
mkdir "$bench_out_eid"

# Book-keeping files reused across runs:
#   SILENT-ID.txt      – operation IDs whose own round-off error is silenced
#   PROBE-ID.txt       – operation IDs whose residue is recorded in this run
#   TEMP_OVERRIDE.bin  – overridden residue tuples for this run
#   OVERRIDE.bin       – cumulative overridden residue tuples from all past runs
#   COUNT.txt          – number of operations silenced/overridden in each run

pushd "$RESULT_DIR"
: > SILENT-ID.txt
: > PROBE-ID.txt
: > TEMP_OVERRIDE.bin
: > OVERRIDE.bin
: > COUNT.txt
popd

# Get HRE-MPFR.txt
build_wasm3 "-DUSE_MPFR"
export WASM3_EOP_LOG=1
if [[ "${ENABLE_FULL_LOG:-0}" == "1" ]]; then
    export WASM3_FULL_LOG=1
fi
rc=0
full_logfile="$bench_out_eid"/FULL-LOG-MPFR.txt
stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str \
    > "$bench_out_eid"/HRE-MPFR.txt \
    2> "$full_logfile" || rc=$?
unset WASM3_EOP_LOG
unset WASM3_FULL_LOG

if [[ ! -s "$full_logfile" ]]; then
    rm -f "$full_logfile"
fi

record_fp_cycles "MPFR"

if (( rc == 137 )); then
    echo "fatal: wasm3 was killed by SIGKILL (likely OOM)" >&2
    exit 1
fi
unset WASM3_EOP_LOG

i=1 # Run counter
limit=${EXE_LIMIT:-5}

build_wasm3 "-DUSE_EID"
while true; do
    exit_code=0
    cur_dir="$bench_out_eid"/"EID-RUN-${i}" # Result directory for this run
    mkdir "$cur_dir"
    logfile="$cur_dir"/"LOG.txt"
    full_logfile="$cur_dir"/"FULL-LOG-EID.txt"

  # BEGIN BLOCK: Execute program to collect HREs & Absorptions
    save_orig_results
    export WASM3_EOP_LOG=1
    if [[ "${ENABLE_FULL_LOG:-0}" == "1" ]]; then
        export WASM3_FULL_LOG=1
    fi
    stdbuf -o0 -e0 "$BUILD_DIR"/wasm3 $input_str \
        2> "$full_logfile" \
        | tee "$logfile" > /dev/null \
        || true
    unset WASM3_EOP_LOG
    unset WASM3_FULL_LOG

    if [[ ! -s "$full_logfile" ]]; then
        rm -f "$full_logfile"
    # else
    #     "$SCRIPT_DIR/depgraph.py" "$full_logfile" 10 > "$cur_dir/g.dot"
    #     dot -Tpng "$cur_dir/g.dot" -o "$cur_dir/g.png"
    fi
    
    # timing
    record_fp_cycles "PRO Round $i"

    abort_if_sigabrt "${PIPESTATUS[0]}" # Only needed here since later are reruns

    "$SCRIPT_DIR"/parse_absorption_e2id.py "$logfile" || exit_code=$?

    unset WASM3_EOP_LOG
    save_new_results
  # END BLOCK

    # Each sub-directory has its own copy of HRE-MPFR.txt
    cp "$bench_out_eid"/HRE-MPFR.txt "$cur_dir"/HRE-MPFR.txt

    # Get FP/FN result
    "$SCRIPT_DIR"/diff_ids.py "$cur_dir" HRE-EID.txt HRE-MPFR.txt

    # Get detailed operation log for FP/FN
    cp "$cur_dir"/FN-ID.txt "$RESULT_DIR"/LOG-FN-ID.txt
    cp "$cur_dir"/FP-ID.txt "$RESULT_DIR"/LOG-FP-ID.txt

    # LOGGING FP/FN OPERATIONS START
    if [[ "${ENABLE_FPN_LOG:-0}" == "1" ]]; then
        # MPFR mode
        build_wasm3 "-DUSE_MPFR"

        stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str \
          | tee  >(grep -E '^\(FN\)' >"$cur_dir/FN-MPFR-LOG.txt") \
          | tee  >(grep -E '^\(FP\)' >"$cur_dir/FP-MPFR-LOG.txt") \
          > /dev/null \
          || true
        
        # EID mode
        build_wasm3 "-DUSE_EID"

        recover_orig_results
        stdbuf -o0 "$BUILD_DIR"/wasm3 $input_str \
          | tee  >(grep -E '^\(FN\)' >"$cur_dir/FN-EID-LOG.txt") \
          | tee  >(grep -E '^\(FP\)' >"$cur_dir/FP-EID-LOG.txt") \
          > /dev/null \
          || true
        recover_new_results
    fi
    # LOGGING FP/FN OPERATIONS END

    # Clear log files for next run
    : > "$RESULT_DIR"/LOG-FN-ID.txt
    : > "$RESULT_DIR"/LOG-FP-ID.txt

    if (( i == limit - 1 && exit_code == 0 )); then
        printf '(Force Override) ' | tee -a "$RESULT_DIR"/COUNT.txt
        exit_code=1
    fi

    if (( i == limit )); then
        exit_code=2
    fi

    case "$exit_code" in
        0)
            (
                cd $RESULT_DIR
                printf 'Run #%d: silence %d\n' "$i" "$(wc -l < SILENT-ID.txt)" | tee -a COUNT.txt
                : > TEMP_OVERRIDE.bin # Empty temporary override values
            )
            ((i++))
            continue
            ;;
        1)
            (
                cd $RESULT_DIR
                printf 'Run #%d: override %d\n' "$i" "$(wc -l < PROBE-ID.txt)" | tee -a COUNT.txt
                tee -a OVERRIDE.bin < TEMP_OVERRIDE.bin >/dev/null
                : > TEMP_OVERRIDE.bin
                : > SILENT-ID.txt
                : > PROBE-ID.txt
            )
            ((i++))
            continue
            ;;

        2)
            final_result="$bench_out_eid"/RESULT.txt
            printf 'MPFR Prec: %d\n' "${MPFR_PREC:=512}" | tee "$final_result" >/dev/null
            printf '#Iter: %d\n' "$i" | tee -a "$final_result" >/dev/null
            tee -a "$final_result" < "$cur_dir"/RESULT.txt >/dev/null
            break
            ;;
    esac
done

# cleanup
mv "$bench_out_eid"/HRE-MPFR.txt "$bench_out_eid"/../
pushd $RESULT_DIR
mv COUNT.txt "$bench_out_eid"/COUNT.txt
rm SILENT-ID.txt
rm PROBE-ID.txt
rm TEMP_OVERRIDE.bin
rm OVERRIDE.bin
rm *-ORIG.txt
rm *-ORIG.bin
rm *-NEW.txt
rm *-NEW.bin
rm LOG-FN-ID.txt
rm LOG-FP-ID.txt
popd
