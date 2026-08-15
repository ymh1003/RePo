# Describe how we obtained the WASM files for the evaluation

## Don't forget
Add
    `#define printf(...) (0)`
to each test program to disable floating-point error checking within `printf` calls.

## Preliminary
Run
    `source ~/emsdk/emsdk_env.sh`
to enable emscripten (compile C/C++ to Wasm).

## Commands for benchmarks in Eval
For test programs in the Microbench suite, use the following command
    `emcc [prog.c] -o [prog.wasm]`
to compile `prog.c` to `prog.wasm`.