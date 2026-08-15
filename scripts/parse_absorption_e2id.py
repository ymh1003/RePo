#!/usr/bin/env python3
"""
(Current in use)
Goal: To parse log file with absorptions considering second largest error ids,
        meanwhile write to HRE-EID.txt (parse reported HREs)
Usage:
    ./parse_absorption_e2id.py <input-file>
E.g., 
    ./parse_absorption_e2id.py EID-RUN-LOG.txt

Read an input text file line-by-line.
For every line of the form
    Absorption: <n1>, <n2>, <n3>
"""

from __future__ import annotations
import re
import os, sys
from pathlib import Path
from contextlib import nullcontext

# Format: Absorption: (%d,%d), (%d,%d), %d
absorb_pattern = re.compile(r'^Absorption:\s*\((-?\d+),(-?\d+)\),\s*\((-?\d+),(-?\d+)\),\s*(-?\d+)\s*$')
# Format: ID: %d
id_pattern = re.compile(r'ID:\s*(\d+)')

def valid_id(i: int) -> bool:
    return i > 0

def write_to_silent_and_probe(inp_path):
    inp_path = Path(inp_path)
    hre_path = inp_path.parent/"HRE-EID.txt"
    absorb_exist = False
    
    max_err_ids, sec_err_ids = set(), set()
    
    try:
        result_dir = os.environ["RESULT_DIR"]
    except KeyError:
        sys.exit("RESULT_DIR is not set")

    silent_path = Path(result_dir)/"SILENT-ID.txt"
    probe_path = Path(result_dir)/"PROBE-ID.txt"
    
    sids = set()
    with silent_path.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                sids.add(int(line))
    """
    orig_probe == []: first run; create/overwrite both output files
    orig_probe != []: subsequent run; append only when current op_id is in orig_probe
    Return codes:
        0  at least one matching absorption written
        1  no match in subsequent run (time to commit)
        2  no absorption lines at all in fresh run
    """
    
    try:
        with open(probe_path, encoding="utf-8") as f:
            orig_probe = {int(line) for line in f if line.strip()}
    except FileNotFoundError:
        orig_probe = set()

    # choose file modes once
    mode_hre = "w"
    mode_sil = "a" if orig_probe else "w"
    mode_pro = "w" if not orig_probe else None

    with inp_path.open(encoding="utf-8") as fin, \
         hre_path.open(mode_hre, encoding="utf-8") as fout_hre, \
         silent_path.open(mode_sil, encoding="utf-8") as fout_sil, \
         (probe_path.open(mode_pro, encoding="utf-8") if mode_pro else nullcontext()) as fout_pro:

        for line in fin:
            m = absorb_pattern.match(line)
            if not m: # Not an absorption
                n = id_pattern.match(line)
                if n:
                    fout_hre.write(line)
                continue
            
            x_id, x_id_aux, y_id, y_id_aux, z_id = map(int, m.groups())
            
            if not orig_probe: # new run (orig_probe is empty)
                if (not ({x_id, y_id} & sec_err_ids)) and (not ({x_id_aux, y_id_aux} & max_err_ids)):
                    if valid_id(x_id) and (x_id not in sids):
                        absorb_exist = True
                        sids.add(x_id)
                        fout_sil.write(f"{x_id}\n")
                    if valid_id(y_id) and (y_id not in sids):
                        absorb_exist = True
                        sids.add(y_id)
                        fout_sil.write(f"{y_id}\n")
                    
                    fout_pro.write(f"{z_id}\n")

                    # Add only valid IDs to sets
                    max_err_ids |= {i for i in (x_id, y_id) if valid_id(i)}
                    sec_err_ids |= {i for i in (x_id_aux, y_id_aux) if valid_id(i)}
                else:
                    continue
            else: # subsequent run (orig_probe is not empty)
                if int(z_id) in orig_probe:
                    if (not {x_id, y_id} & sec_err_ids) and (not {x_id_aux, y_id_aux} & max_err_ids):
                        if valid_id(x_id) and (x_id not in sids):
                            absorb_exist = True
                            fout_sil.write(f"{x_id}\n")
                            sids.add(x_id)
                        if valid_id(y_id) and (y_id not in sids):
                            absorb_exist = True
                            fout_sil.write(f"{y_id}\n")
                            sids.add(y_id)
                        
                        # Add only valid IDs to sets
                        max_err_ids |= {i for i in (x_id, y_id) if valid_id(i)}
                        sec_err_ids |= {i for i in (x_id_aux, y_id_aux) if valid_id(i)}
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