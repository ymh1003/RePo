KERNELS=ContinuedFraction1 MindlessG ParallelResistance SimpsonsRule SinTaylorExpansion cc compare-zero-self cordic_cos cordic_sin diff-roots-simple diff-roots diff-simple dotproduct1 dotproduct2 exact-ranges gromacs-bondfree-580 gsl-modpi indirect indirect1 mini muller multi-args naturalnumber newton newtonraphson newtonraphsonsqrt onthefly redir rootfinding1 rootfinding2 same-small second_order simpl-swallow-testa simpl-swallow-testb simpl-swallow-testc simpl-swallow-testd simple-negate small some-trig sqrtf sum-50 sum-500 tiny var-test-2 var-test
SCRIPT=$(ROOT_DIR)/scripts/run_bench_together.sh
BENCH_RES=$(KERNELS:%=microbench/%)

.PHONY: all clean
all : $(BENCH_RES)

microbench/diff-roots : $(ROOT_DIR)/eftSan_bench/microbench/diff-roots.wasm
	@echo "$@: " >&2
	MPFR_PREC=1024 $(SCRIPT) "$<" "$@"

microbench/% : $(ROOT_DIR)/eftSan_bench/microbench/%.wasm
	@echo "$@: " >&2
	$(SCRIPT) "$<" "$@"

clean :
	rm -rf $(ROOT_DIR)/report/microbench