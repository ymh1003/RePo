#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

def read_ids(path):
    """
    Read lines of the form "ID: [number]" and return a set of integers.
    Ignores malformed lines.
    """
    id_pattern = re.compile(r'ID:\s*(\d+)')
    ids = set()
    with open(path, 'r') as f:
        for _, line in enumerate(f, 1):
            match = id_pattern.search(line)
            if match:
                ids.add(int(match.group(1)))
    return ids

def main():
    parser = argparse.ArgumentParser(
        description="Compute IDs present in one file but not the other."
    )
    parser.add_argument('dir', help ="Directory containing the files")
    parser.add_argument('file1', help="Path to first file")
    parser.add_argument('file2', help="Path to second file (Oracle)")
    args = parser.parse_args()

    ids1 = read_ids(Path(args.dir) / args.file1)
    ids2 = read_ids(Path(args.dir) / args.file2)

    only_in_1 = sorted(ids1 - ids2)
    only_in_2 = sorted(ids2 - ids1)
    in_both   = sorted(ids1 & ids2)
    
    with open(Path(args.dir) / "RESULT.txt", "w") as f:
        f.write(f"Number of false positives: {len(only_in_1)}\n")
        f.write(f"Number of false negatives: {len(only_in_2)}\n")
        f.write(f"Number of true positives:  {len(in_both)}\n")

    with open(Path(args.dir) / "FP-ID.txt", "w") as f:
        for i in only_in_1:
            f.write(f"{str(i)}\n")

    with open(Path(args.dir) / "FN-ID.txt", "w") as f:
        for i in only_in_2:
            f.write(f"{str(i)}\n")

if __name__ == "__main__":
    main()