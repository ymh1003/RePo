ALL_KERNELS := $(basename $(notdir $(wildcard $(ROOT_DIR)/fpbench-wasm/*.wasm)))
EXCLUDED := precimonious-0 precimonious-1 salsa-5 salsa-6 salsa-8 rosa-35 # timeout
KERNELS := $(filter-out $(EXCLUDED), $(ALL_KERNELS))

SCRIPT=$(ROOT_DIR)/scripts/run_bench_together.sh
BENCH_RES=$(KERNELS:%=fpbench/%)

.PHONY: all clean
all : $(BENCH_RES)

fpbench/% : $(ROOT_DIR)/fpbench-wasm/%.wasm
	@echo "$@: " >&2
	MPFR_PREC=4096 $(SCRIPT) "$<" "$@"

clean :
	rm -rf $(ROOT_DIR)/report/fpbench