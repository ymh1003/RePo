#!/bin/bash
source ~/emsdk/emsdk_env.sh

for file in *.c; do
    if [[ "$file" == "test.c" ]]; then
        continue
    fi

    base_name="${file%.c}"

    emcc "$file" -o "$base_name.wasm"

    # Check if the compilation was successful
    if [[ $? -eq 0 ]]; then
        echo "Successfully compiled $file to $base_name.wasm"
    else
        echo "Failed to compile $file"
    fi
done