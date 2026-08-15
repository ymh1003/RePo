#!/usr/bin/env python3
"""
Goal: To parse log file with absorptions in batch, determined by [batch_size]
Note: Does NOT support second largest error id

Usage:
    ./parse_absorption_batch.py <input-file>
E.g., 
    ./parse_absorption_batch.py EID-RUN-LOG.txt

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
        orig_probe = set({int(line) for line in f if line.strip()})
    
    rx = re.compile(r'^Absorption:\s*(\d+),\s*(\d+),\s*(\d+)\s*$')

    """
    orig_probe == []: first run; create/overwrite both output files
    orig_probe != []: subsequent run; append only when current op_id is in orig_probe
    Return codes:
        0  at least one matching absorption written
        1  no match in subsequent run (time to commit)
        2  no absorption lines at all in fresh run
    """
    
    absorb_exist = False

    # choose file modes once
    mode_sil = "a" if orig_probe else "w"
    mode_pro = "w" if not orig_probe else None

    batch_size = 50 # Set batch size for processing absorptions
    
    with inp_path.open(encoding="utf-8") as fin, \
         silent_path.open(mode_sil, encoding="utf-8") as fout_sil, \
         (probe_path.open(mode_pro, encoding="utf-8") if mode_pro else nullcontext()) as fout_pro:

        lines = fin.readlines()
        # lines = lines[::-1] # reverse order
        for line in lines:
            m = rx.match(line)
            if not m: 
                continue
            n1, n2, n3 = m.groups()
            
            if not orig_probe: # new run (orig_probe is empty)
                if batch_size > 0:
                    new_absorb += 2
                    absorb_exist = True
                    fout_sil.write(f"{n1}\n{n2}\n")
                    fout_pro.write(f"{n3}\n")
                    batch_size -= 1
                    print(f"New absorption: {n1}, {n2}, {n3}")
                else:
                    break
            else: # subsequent run (orig_probe is not empty)
                if int(n3) in orig_probe:
                    new_absorb += 2
                    absorb_exist = True
                    fout_sil.write(f"{n1}\n{n2}\n")
                    print(f"Add-on silence for {n3}: {n1}, {n2}")
                else:
                    continue

    if orig_probe:
        return 0 if absorb_exist else 1
    else:
        return 0 if absorb_exist else 2

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(f"Usage: {sys.argv[0]} <input-file>")
        
    exit_code = write_to_silent_and_probe(sys.argv[1])
    print(f"Exit code: {exit_code}")
    sys.exit(exit_code)