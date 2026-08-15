#!/usr/bin/env bash

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd -P)"
ROOT_DIR="${ROOT_DIR:-$SCRIPT_DIR}"

if [[ $# -gt 1 ]]; then
    echo "Usage: $0 [report-dir]" >&2
    exit 2
fi

REPORT_DIR="${1:-${REPORT_DIR:-$ROOT_DIR/report}}"
if [[ ! -d "$REPORT_DIR" ]]; then
    echo "Report directory not found: $REPORT_DIR" >&2
    exit 1
fi

REPORT_DIR="$(cd "$REPORT_DIR" && pwd -P)"
HTML_OUT="$REPORT_DIR/index.html"
LIMIT="${LIMIT:-${EXE_LIMIT:-N/A}}"

# ---------------------------------------------------------------------------
# 0. Helpers
# ---------------------------------------------------------------------------
html_escape() {
    sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g'
}

parse_counts() {
    local file=$1 fp fn
    [[ -f $file ]] || { echo "NA NA"; return; }
    fp=$(grep -i 'false positives' "$file" | grep -o '[0-9]\+' | head -n1)
    fn=$(grep -i 'false negatives' "$file" | grep -o '[0-9]\+' | head -n1)
    echo "${fp:-0} ${fn:-0}"
}

parse_tp() {
    local file=$1 tp
    [[ -f $file ]] || { echo "NA"; return; }
    tp=$(grep -i 'true positives' "$file" | grep -o '[0-9]\+' | head -n1)
    echo "${tp:-NA}"
}

# summarize FP/FN as text (or ✓ / N/A), using parse_counts
parse_result() {
    local file=$1 fp fn
    [[ -f $file ]] || { echo "N/A"; return; }

    read -r fp fn <<<"$(parse_counts "$file")"
    if (( fp == 0 && fn == 0 )); then
        printf '✓'
    else
        local parts=()
        (( fp != 0 )) && parts+=("${fp} FP")
        (( fn != 0 )) && parts+=("${fn} FN")
        (IFS=', '; printf '%s' "${parts[*]}")
    fi
}

# build " (P=..., R=...)" if TP present; else empty — reuse helpers
pr_paren() {
    local file=$1
    [[ -f $file ]] || { echo ""; return; }

    local fp fn tp
    read -r fp fn <<<"$(parse_counts "$file")"
    tp="$(parse_tp "$file")"

    # convert "NA" or empty to 0
    [[ "$tp" == "NA" || -z "$tp" ]] && { echo ""; return; }
    [[ "$fp" == "NA" || -z "$fp" ]] && fp=0
    [[ "$fn" == "NA" || -z "$fn" ]] && fn=0

    awk -v tp="$tp" -v fp="$fp" -v fn="$fn" 'BEGIN{
        p_den = tp + fp;
        r_den = tp + fn;
        if (p_den > 0) { p = tp / p_den; p_str = sprintf("P=%.3f", p); } else { p_str = "P=N/A"; }
        if (r_den > 0) { r = tp / r_den; r_str = sprintf("R=%.3f", r); } else { r_str = "R=N/A"; }
        printf(" (%s, %s)", p_str, r_str);
    }'
}

# combine + escape for HTML
result_with_pr() {
    local file=$1
    printf '%s%s\n' "$(parse_result "$file")" | html_escape
}

parse_iter() {
    local file=$1 it
    [[ -f $file ]] || { echo "N/A" ; return; }
    it=$(grep -i '^#Iter:' "$file" | grep -o '[0-9]\+' || true)
    echo "${it:-?}"
}

parse_prec() {
    local file=$1 it
    [[ -f $file ]] || { echo "N/A" ; return; }
    it=$(grep -i '^MPFR Prec:' "$file" | grep -o '[0-9]\+' || true)
    echo "${it:-?}"
}

is_strictly_better() {
  local pro_fp=$1 pro_fn=$2 oth_fp=$3 oth_fn=$4
  [[ $pro_fp == NA || $pro_fn == NA || $oth_fp == NA || $oth_fn == NA ]] && return 1
  (( pro_fp <= oth_fp && pro_fn <= oth_fn && (pro_fp < oth_fp || pro_fn < oth_fn) ))
}

is_strictly_worse() {
  local pro_fp=$1 pro_fn=$2 oth_fp=$3 oth_fn=$4
  [[ $pro_fp == NA || $pro_fn == NA || $oth_fp == NA || $oth_fn == NA ]] && return 1
  (( pro_fp >= oth_fp && pro_fn >= oth_fn && (pro_fp > oth_fp || pro_fn > oth_fn) ))
}

is_mixed() {
  local pro_fp=$1 pro_fn=$2 oth_fp=$3 oth_fn=$4
  [[ $pro_fp == NA || $pro_fn == NA || $oth_fp == NA || $oth_fn == NA ]] && return 1

  # Case 1: FP better, FN worse
  if (( pro_fp < oth_fp && pro_fn > oth_fn )); then
    return 0
  fi

  # Case 2: FN better, FP worse
  if (( pro_fn < oth_fn && pro_fp > oth_fp )); then
    return 0
  fi

  return 1
}

# --- Helpers for parsing FP cycle counts from TIME.txt ---------------------

get_block_cycles() {
    local file=$1 label=$2
    [[ -f $file ]] || { echo "N/A"; return; }
    awk -v label="$label" '
        # Start of the block: e.g., "MPFR:"
        $0 ~ "^"label":" { inblk=1; next }
        # Any later section label ends the block
        inblk && /^[A-Za-z].*:$/ {
            inblk = 0
        }
        # First FP_CYCLES line after the label
        inblk && /^FP_CYCLES:[[:space:]]+/ {
            print $2
            exit
        }
    ' "$file"
}

get_pro_max_cycles() {
    local file=$1
    [[ -f $file ]] || { echo "N/A"; return; }
    awk '
        # Start of a PRO round block, e.g. "PRO Round 1:"
        /^PRO Round / {
            inblk = 1
            next
        }
        # Any other section label like "MPFR:", "DD:", "EFTSan:" ends the PRO block
        /^[A-Za-z].*:$/ {
            inblk = 0
        }
        # Collect cycle counts only while in a PRO block
        inblk && /^FP_CYCLES:[[:space:]]+/ {
            cycles = $2 + 0
            if (cycles > max) { max = cycles; max_str = $2 }
        }
        END {
            if (max_str == "") print "N/A";
            else print max_str;
        }
    ' "$file"
}

# ---------------------------------------------------------------------------
# 1. Header
# ---------------------------------------------------------------------------
cat >"$HTML_OUT" <<HTML
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>RePo Experiment Report</title>
  <style>
    body{font-family:sans-serif;padding:2em}
    a{font-size:1.05em}
    table{border-collapse:collapse;margin:1em 0;width:100%}
    th,td{border:1px solid #888;padding:.4em;text-align:left}
    th{background:#e0e0e0}
    .green{color:#2e7d32;font-weight:600}
    .orange{color:#ef6c00;font-weight:600}
    .red{color:#c62828;font-weight:600}
    .black{color:#000;font-weight:600}
    .grey{color:#616161;font-weight:600}
    .blue{color:#0d47a1;font-weight:600}
    .summary{padding:.75em 1em;background:#f5f5f5;border:1px solid #ddd;border-radius:.4em}
  </style>
</head>
<body>
  <h1>RePo Experiment Report</h1>
  <p> Re-evaluation Limit: <strong>${LIMIT:-N/A}</strong> </p>
  <p> Second-order error in MUL: <strong>${SECOND_ORDER_ERR:-N/A}</strong> </p>
  <p> Condition number threshold: <strong>2^${COND_THRESH:-N/A}</strong> </p>
  <p> Timing: <a href="time.log">LOG</a> </p>

HTML

# Portable mktemp: works on nightly and macOS
mktemp_tables() {
  local tmpl="pro_report_tables.XXXXXX"
  mktemp -t "$tmpl" 2>/dev/null || mktemp "${TMPDIR:-/tmp}/$tmpl"
}
TABLES_TMP="$(mktemp_tables)"

count_win=0
count_mix=0
count_loss=0
count_normal=0

# ---------------------------------------------------------------------------
# 2. Tables
# ---------------------------------------------------------------------------
cd "$REPORT_DIR"

for S in *; do
  [[ -d "$S" ]] || continue

  # Per-suite timing rows for the new timing table
  timing_rows=""

  {
    printf '\n  <h2>%s</h2>\n' "$S"
    printf '  <table>\n    <tr><th>Benchmarks</th><th>Raw Data</th><th>EFTSan-Fixed</th><th>PRO Start</th><th>PRO End</th><th>#RE</th><th>#RO</th><th>QD</th><th>MPFR Precision</th><th>Timing</th></tr>\n'
  } >>"$TABLES_TMP"

  for B in "$S"/*; do
    [[ -d "$B" ]] || continue

    B_NAME=$(basename "$B")
    B_DIR="<a href=\"${B}/\">.</a>"
    TIME_FILE="$B/TIME.txt"
    if [[ -f "$TIME_FILE" ]]; then
      TIME_LINK="<a href=\"${TIME_FILE}\">.</a>"
    else
      TIME_LINK="N/A"
    fi

    EFT_FILE="$B/eft/RESULT.txt"
    DD_FILE="$B/dd/RESULT.txt"
    PRO_START_FILE="$B/eid/EID-RUN-1/RESULT.txt"
    PRO_FINAL_FILE="$B/eid/RESULT.txt"
    RO_FILE="$B/eid/COUNT.txt"
    MPFR_DD_FILE="$B/mpfr-dd/RESULT.txt"
    QD_FILE="$B/qd/RESULT.txt"

    EFT_RES=$(result_with_pr "$EFT_FILE")
    DD_RES=$(result_with_pr "$DD_FILE")
    PRO_START=$(result_with_pr "$PRO_START_FILE")
    PRO_RES=$(result_with_pr "$PRO_FINAL_FILE")
    QD_RES=$(result_with_pr "$QD_FILE")

    PRO_ITER=$(parse_iter "$PRO_FINAL_FILE"  | html_escape)
    MPFR_DD_RES=$(parse_result "$MPFR_DD_FILE" | html_escape)
    PREC_BITS=$(parse_prec "$PRO_FINAL_FILE" | html_escape)

    read -r PRO_FP PRO_FN <<<"$(parse_counts "$PRO_FINAL_FILE")"
    read -r DD_FP  DD_FN  <<<"$(parse_counts "$DD_FILE")"
    read -r MP_FP  MP_FN  <<<"$(parse_counts "$MPFR_DD_FILE")"
    read -r QD_FP  QD_FN  <<<"$(parse_counts "$QD_FILE")"

    # -----------------------------------------------------------------------
    # Coloring classification ONLY (NEW rubric; does NOT change LaTeX rubrics)
    # -----------------------------------------------------------------------

    # Compare only PRO vs QD for green/orange/red
    pro_better=false
    pro_mixed=false
    pro_worse=false

    if is_strictly_better "$PRO_FP" "$PRO_FN" "$QD_FP" "$QD_FN"; then
      pro_better=true
    elif is_mixed "$PRO_FP" "$PRO_FN" "$QD_FP" "$QD_FN"; then
      pro_mixed=true
    elif is_strictly_worse "$PRO_FP" "$PRO_FN" "$QD_FP" "$QD_FN"; then
      pro_worse=true
    fi

    # Black if DD, PRO, QD are all already perfect (ignore MPFR-DD)
    all_zero=false
    if [[ $PRO_FP != NA && $PRO_FN != NA && $DD_FP != NA && $DD_FN != NA && $QD_FP != NA && $QD_FN != NA ]]; then
      if (( PRO_FP==0 && PRO_FN==0 && DD_FP==0 && DD_FN==0 && QD_FP==0 && QD_FN==0 )); then
        all_zero=true
      fi
    fi

    # Grey if everything is N/A (include QD)
    all_na=false
    if [[ $(echo "$EFT_RES" | tr -d '[:space:]') == "N/A" && \
          $(echo "$DD_RES" | tr -d '[:space:]') == "N/A" && \
          $(echo "$PRO_START" | tr -d '[:space:]') == "N/A" && \
          $(echo "$PRO_RES" | tr -d '[:space:]') == "N/A" && \
          $(echo "$QD_RES" | tr -d '[:space:]') == "N/A" && \
          $PRO_ITER == "N/A" && \
          $MPFR_DD_RES == "N/A" && \
          ! -f $RO_FILE ]]; then
      all_na=true
    fi

    # Color assignment
    if $all_na; then
      css_class="grey"
    elif $all_zero; then
      css_class="black"; ((count_normal++))
    elif $pro_better; then
      css_class="green"; ((count_win++))
    elif $pro_mixed; then
      css_class="orange"; ((count_mix++))
    elif $pro_worse; then
      css_class="red"; ((count_loss++))
    else
      css_class="blue"   # dark blue: any other case
    fi

    B_DISPLAY="<span class=\"$css_class\">$B_NAME</span>"

    if [[ -f $RO_FILE ]]; then
      RO_COUNT="<a href=\"${B}/eid/COUNT.txt\">.</a>"
    else
      RO_COUNT="N/A"
    fi

    printf '    <tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n' \
           "$B_DISPLAY" "$B_DIR" "$DD_RES" "$PRO_START" "$PRO_RES" "$PRO_ITER" "$RO_COUNT" "$QD_RES" "$PREC_BITS" "$TIME_LINK" >>"$TABLES_TMP"

    # --- Build timing row for the new timing table ------------------------
    if [[ -s "$TIME_FILE" ]]; then
      MPFR_TIME=$(get_block_cycles "$TIME_FILE" "MPFR")
      EFTSAN_TIME=$(get_block_cycles "$TIME_FILE" "EFTSan")
      U_TIME=$(get_block_cycles "$TIME_FILE" "Uninstrumented")
      DD_TIME=$(get_block_cycles "$TIME_FILE" "DD")
      QD_TIME=$(get_block_cycles "$TIME_FILE" "QD")
      PRO_TIME=$(get_pro_max_cycles "$TIME_FILE")
    else
      MPFR_TIME="N/A"
      EFTSAN_TIME="N/A"
      U_TIME="N/A"
      DD_TIME="N/A"
      QD_TIME="N/A"
      PRO_TIME="N/A"
    fi

    timing_rows+=$(printf '    <tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n' \
      "$B_DISPLAY" "$U_TIME" "$DD_TIME" "$PRO_TIME" "$QD_TIME" "$MPFR_TIME")

  done

  # Close main results table
  printf '  </table>\n' >>"$TABLES_TMP"

  # Emit timing summary table for this suite if we collected any rows
  if [[ -n "$timing_rows" ]]; then
    {
      printf '  <table>\n'
      printf '    <tr><th>Benchmarks</th><th>Uninstrumented FP Cycles</th><th>EFTSan-Fixed FP Cycles</th><th>Max PRO FP Cycles</th><th>QD FP Cycles</th><th>MPFR FP Cycles</th></tr>\n'
      printf '%s' "$timing_rows"
      printf '  </table>\n'
    } >>"$TABLES_TMP"
  fi

done

# ---------------------------------------------------------------------------
# 3. Summary + close
# ---------------------------------------------------------------------------
cat >>"$HTML_OUT" <<HTML
  <div class="summary">
    <strong>Win summary:</strong>
    Win = $count_win, Mix = $count_mix, Loss = $count_loss, Normal = $count_normal.
  </div>

HTML

cat "$TABLES_TMP" >>"$HTML_OUT"
rm -f "$TABLES_TMP"

cat >>"$HTML_OUT" <<HTML
</body>
</html>
HTML

echo "Landing page written to $HTML_OUT"
