KERNELS=hotspot kmeans myocyte backprop lavamd lud particlefilter sradv2 hotspot3d nn
SCRIPT=$(ROOT_DIR)/scripts/run_bench_together.sh
BENCH_RES=$(KERNELS:%=rodinia/%)

.PHONY: all clean
all : $(BENCH_RES)

# hotspot
rodinia/hotspot: $(ROOT_DIR)/rodinia-wasm/hotspot
	@( \
	  cd "$<" && \
	  echo "$@: " >&2 && \
	  MPFR_PREC=2048 $(SCRIPT) "./hotspot.wasm 512 512 2 1 temp_512.txt power_512.txt none" "$@" \
	)

# kmeans
rodinia/kmeans: $(ROOT_DIR)/rodinia-wasm/kmeans
	@( \
	  cd "$<" && \
	  echo "$@: " >&2 && \
	  $(SCRIPT) "./kmeans_serial.wasm -i data.txt" "$@" \
	)

# myocyte
rodinia/myocyte: $(ROOT_DIR)/rodinia-wasm/myocyte
	@( \
	  cd "$<" && \
	  echo "$@: " >&2 && \
	  $(SCRIPT) "--stack-size 1000000 ./myocyte.wasm 100 1 0 4" "$@" \
	)

# streamcluster
rodinia/streamcluster: $(ROOT_DIR)/rodinia-wasm/streamcluster
	@( \
	  cd "$<" && \
	  echo "$@: " >&2 && \
	  $(SCRIPT) "./streamcluster_original.wasm 10 20 256 4096 4096 1000 none none 1" "$@" \
	)

# backprop
rodinia/backprop: $(ROOT_DIR)/rodinia-wasm/backprop
	@( \
	  cd "$<" && \
	  echo "$@: " >&2 && \
	  MPFR_PREC=1024 $(SCRIPT) "./backprop.wasm 2048" "$@" \
	)

# lavamd
rodinia/lavamd: $(ROOT_DIR)/rodinia-wasm/lavamd
	@( \
	  cd "$<" && \
	  echo "$@: " >&2 && \
	  MPFR_PREC=2048 $(SCRIPT) "./lavamd.wasm" "$@" \
	)

# lud
rodinia/lud: $(ROOT_DIR)/rodinia-wasm/lud
	@( \
	  cd "$<" && \
	  echo "$@: " >&2 && \
	  $(SCRIPT) "./lud.wasm -s 100" "$@" \
	)

# particlefilter
rodinia/particlefilter: $(ROOT_DIR)/rodinia-wasm/particlefilter
	@( \
	  cd "$<" && \
	  echo "$@: " >&2 && \
	  MPFR_PREC=2048 $(SCRIPT) "./particlefilter.wasm -x 128 -y 128 -z 10 -np 100" "$@" \
	)

# sradv2
rodinia/sradv2: $(ROOT_DIR)/rodinia-wasm/sradv2
	@( \
	  cd "$<" && \
	  echo "$@: " >&2 && \
	  $(SCRIPT) "./srad_v2.wasm 128 128 0 127 0 127 2 0.5 2" "$@" \
	)

# hotspot3d
rodinia/hotspot3d: $(ROOT_DIR)/rodinia-wasm/hotspot3d
	@( \
	  cd "$<" && \
	  echo "$@: " >&2 && \
	  $(SCRIPT) "./hotspot3d.wasm 64 8 10 power_64x8.txt temp_64x8.txt none" "$@" \
	)

# nn
rodinia/nn: $(ROOT_DIR)/rodinia-wasm/nn
	@( \
	  cd "$<" && \
	  echo "$@: " >&2 && \
	  $(SCRIPT) "./nn.wasm filelist_4 3 30 90" "$@" \
	)

clean :
	rm -rf $(ROOT_DIR)/report/rodinia