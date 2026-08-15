#!/usr/bin/env python3
import re
import sys
from dataclasses import dataclass
from typing import Dict, List, Optional

# ---------------- formatting ----------------

def fmt_num(s: str, digits: int = 6) -> str:
    """Compact numeric format; keep SPECIAL/-SPECIAL verbatim."""
    s = s.strip()
    if s in ("SPECIAL", "-SPECIAL"):
        return s
    try:
        return f"{float(s):.{digits}e}"
    except ValueError:
        return s

def esc_dot_label(s: str) -> str:
    """Escape a string for DOT quoted labels."""
    return s.replace("\\", "\\\\").replace('"', '\\"')

# ---------------- data types ----------------

@dataclass
class VarRef:
    fp: str
    err: str
    gen: int  # generator op id (e->op)

@dataclass
class OpRecord:
    op_id: int
    op_name: str
    args: List[VarRef]  # unary:1, binary:2
    res: VarRef

# ---------------- parsing ----------------

LINE_RE = re.compile(r'^\(ID\s+(\d+)\)\s+(\S+):\s+(.*)$')

# Match "(<fp>, [<err>, <gen>, ...])"
TUPLE_RE = re.compile(
    r'\(\s*([^,]+?)\s*,\s*'
    r'\[\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)\s*,'
    r'\s*([+-]?\d+)\s*,'
    r'.*?\]\s*\)'
)

def parse_line(line: str, digits: int) -> Optional[OpRecord]:
    line = line.strip()
    if not line:
        return None
    m = LINE_RE.match(line)
    if not m:
        return None

    op_id = int(m.group(1))
    op_name = m.group(2)
    rest = m.group(3)

    triples = TUPLE_RE.findall(rest)
    if not triples:
        return None

    vars_ = [
        VarRef(
            fp=fmt_num(fp, digits),
            err=fmt_num(err, digits),
            gen=int(gen),
        )
        for fp, err, gen in triples
    ]

    if len(vars_) == 2:      # unary: x, z
        return OpRecord(op_id, op_name, [vars_[0]], vars_[1])
    elif len(vars_) == 3:    # binary: x, y, z
        return OpRecord(op_id, op_name, [vars_[0], vars_[1]], vars_[2])
    return None

def load_ops(path: str, digits: int) -> Dict[int, OpRecord]:
    ops: Dict[int, OpRecord] = {}
    with open(path, "r", errors="replace") as f:
        for line in f:
            rec = parse_line(line, digits)
            if rec:
                ops[rec.op_id] = rec
    return ops

# ---------------- DOT emitting ----------------

def emit_dot(ops: Dict[int, OpRecord], target: int, window: int = 50) -> str:
    lo = max(1, target - (window - 1))
    hi = target
    win = {k: v for (k, v) in ops.items() if lo <= k <= hi}

    if target not in win:
        raise SystemExit(f"Target op ID {target} not found in window [{lo}, {hi}]")

    out: List[str] = []
    out.append("digraph deps {")
    out.append("  rankdir=LR;")
    out.append("  node [shape=box, fontsize=10];")

    # op nodes (results)
    for op_id in sorted(win):
        r = win[op_id]
        label = f"ID {r.op_id}\n FP={r.res.fp}\n ERR={r.res.err}"
        out.append(f'  op{op_id} [label="{esc_dot_label(label)}"];')

    # leaf nodes for INPUT/outside deps
    leaf_counter = 0

    for op_id in sorted(win):
        r = win[op_id]
        consumer = r.op_name  # edge label: operator that consumes the value

        for arg in r.args:
            src = arg.gen
            edge_label = consumer  # requested: use operator name, not x/y

            if src in win:
                out.append(f'  op{src} -> op{op_id} [label="{esc_dot_label(edge_label)}"];')
            else:
                leaf_counter += 1
                leaf_id = f"leaf_{op_id}_{leaf_counter}"

                kind = "INPUT" if src == -1 else f"OUTSIDE(ID {src})"
                leaf_label = f"{kind}\n FP={arg.fp}\n ERR={arg.err}"

                out.append(f'  {leaf_id} [shape=ellipse, fontsize=10, label="{esc_dot_label(leaf_label)}"];')
                out.append(f'  {leaf_id} -> op{op_id} [label="{esc_dot_label(edge_label)}"];')

    # highlight target op
    out.append(f"  op{target} [penwidth=3];")
    out.append("}")
    return "\n".join(out)

# ---------------- main ----------------

def main():
    if len(sys.argv) < 3:
        print("Usage: depgraph.py <FULL_LOG.txt> <op_id> [window=50] [digits=6]", file=sys.stderr)
        sys.exit(2)

    path = sys.argv[1]
    target = int(sys.argv[2])
    window = int(sys.argv[3]) if len(sys.argv) >= 4 else 50
    digits = int(sys.argv[4]) if len(sys.argv) >= 5 else 6  # 5–10 recommended

    ops = load_ops(path, digits)
    print(emit_dot(ops, target, window))

if __name__ == "__main__":
    main()