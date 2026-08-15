#!/usr/bin/env python3
"""
gen_debug_ids.py — Read a file, find "OP_ID: N", and print 20
                   equally-spaced integers in [0, N).

Usage:
    ./gen_debug_ids.py input.txt
"""

import re
import sys
from pathlib import Path

PREFIX = "OP_TT:"

def get_op_id(path: Path):
    pat = re.compile(rf"^{re.escape(PREFIX)}\s*(\d+)")
    with path.open() as fp:
        for line in fp:
            m = pat.match(line)
            if m:
                return int(m.group(1))
    return None


def main():
    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} <file>")

    n = get_op_id(Path(sys.argv[1]))

    count = 20
    if n == 0:
        sys.exit(1) # NO FLOATING POINT OPERATIONS

    step = n / count
    marks = [int(i * step) for i in range(count)]

    # Ensure values are strictly less than n and unique
    marks = sorted(set(m for m in marks if m < n))
    while len(marks) < count:      # pad with largest-1, -2, … if needed
        marks.append(marks[-1] - 1)
    marks = marks[:count]

    for m in marks:
        print(m)

    sys.exit(0)


if __name__ == "__main__":
    main()
