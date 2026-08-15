#!/usr/bin/env python3
"""
Goal: To parse log file with absorptions considering all of them
Note: Does NOT support second largest error id

Usage:
    ./parse_absorption_all.py <input-file>
E.g., 
    ./parse_absorption_all.py EID-RUN-LOG.txt

Read an input text file line-by-line.
For every line of the form
    Absorption: <n1>, <n2>, <n3>
"""

from __future__ import annotations
import re
import os, sys
from pathlib import Path
from contextlib import nullcontext

def write_to_silent_and_probe(inp_path):
    inp_path = Path(inp_path)
    absorb_exist = False
    new_absorb = 0
    
    try:
        result_dir = os.environ["RESULT_DIR"]
    except KeyError:
        sys.exit("RESULT_DIR is not set")

    silent_path = Path(result_dir)/"SILENT-ID.txt"
    probe_path = Path(result_dir)/"PROBE-ID.txt"
    
    with open(probe_path, encoding="utf-8") as f:
        probe_list = set({int(line) for line in f if line.strip()})
    
    rx = re.compile(r'^Absorption:\s*(\d+),\s*(\d+),\s*(\d+)\s*$')

    """
    probe_list == []: first run; create/overwrite both output files
    probe_list != []: subsequent run; append only when current op_id is in probe_list
    Return codes:
        0  at least one matching absorption written
        1  no match in subsequent run (time to commit)
        2  no absorption lines at all in fresh run
    """
    
    absorb_exist = False

    # choose file modes once
    mode_sil = "a" if probe_list else "w"
    mode_pro = "w" if not probe_list else None      # None → not opened

    with inp_path.open(encoding="utf-8") as fin, \
         silent_path.open(mode_sil, encoding="utf-8") as fout_sil, \
         (probe_path.open(mode_pro, encoding="utf-8") if mode_pro else nullcontext()) as fout_pro:

        for line in fin:
            m = rx.match(line)
            if not m:
                continue

            n1, n2, n3 = m.groups()

            if not probe_list or int(n3) in probe_list:   # first run || new absorption with the same current op_id
                new_absorb += 2
                absorb_exist = True
                fout_sil.write(f"{n1}\n{n2}\n")
                if fout_pro:                              # only on first run
                    fout_pro.write(f"{n3}\n")
                else:
                    pass
                    # print(f"New absorption found: {n1}, {n2}, {n3}")

    # print(f"Number of New Absorption(s): {new_absorb}")
    if probe_list:
        return 0 if absorb_exist else 1
    else:
        return 0 if absorb_exist else 2

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(f"Usage: {sys.argv[0]} <input-file>")
        
    exit_code = write_to_silent_and_probe(sys.argv[1])
    print(f"Exit code: {exit_code}")
    sys.exit(exit_code)