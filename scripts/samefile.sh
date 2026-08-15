#!/usr/bin/env bash
# Compare two files byte-for-byte.
# Exit 0 if they are identical, 1 if they differ, 2 on usage error.

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 fileA fileB" >&2
    exit 2
fi

if cmp -s -- "$1" "$2"; then
    exit 0
else
    exit 1
fi