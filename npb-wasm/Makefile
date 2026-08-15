KERNELS=bt cg ep ft lu mg sp
SCRIPT=$(ROOT_DIR)/scripts/run_bench_together.sh
BENCH_RES=$(KERNELS:%=npb/%)

.PHONY: all clean
all : $(BENCH_RES)

npb/bt : $(ROOT_DIR)/npb-wasm/bt.wasm
	@echo "$@: " >&2
	MPFR_PREC=128 $(SCRIPT) "--stack-size 1000000 $<" "$@"

npb/ep : # SIGKILL for 128 precision bits
	@echo "Skipping ep"

npb/mg : # 2k+ Re-evaluations

npb/% : $(ROOT_DIR)/npb-wasm/%.wasm
	@echo "$@: " >&2
	$(SCRIPT) "--stack-size 1000000 $<" "$@"

clean :
	rm -rf $(ROOT_DIR)/report/npb