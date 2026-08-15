KERNELS=fdtd-2d bicg heat-3d symm syr2k 3mm gramschmidt syrk mvt adi correlation gesummv trmm gemm cholesky doitgen deriche jacobi-2d ludcmp seidel-2d trisolv durbin atax 2mm lu gemver covariance jacobi-1d
SCRIPT=$(ROOT_DIR)/scripts/run_bench_together.sh
BENCH_RES=$(KERNELS:%=polybench/%)

.PHONY: all clean
all : $(BENCH_RES)

polybench/fdtd-2d : $(ROOT_DIR)/polybench-wasm/fdtd-2d.wasm
	@echo "$@: " >&2
	MPFR_PREC=2048 $(SCRIPT) "$<" "$@"

polybench/gramschmidt : $(ROOT_DIR)/polybench-wasm/gramschmidt.wasm
	@echo "$@: " >&2
	MPFR_PREC=4096 $(SCRIPT) "$<" "$@"

polybench/% : $(ROOT_DIR)/polybench-wasm/%.wasm
	@echo "$@: " >&2
	$(SCRIPT) "$<" "$@"

clean :
	rm -rf $(ROOT_DIR)/report/polybench