ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

.PHONY: nothing
nothing:

.PHONY: clean
clean:
	make -C rtl clean
	make -C de5 clean
	make -C de5/bridge-board clean
	make -C hostlink clean
	make -C include clean
	make -C lib clean
	make -C apps/hello clean
	make -C apps/boot clean
	make -C apps/heat clean
	make -C apps/ping clean
	make -C apps/ring clean
	make -C apps/ring/general clean
	make -C apps/flood1 clean
	make -C apps/flood2 clean
	make -C apps/benchmarks clean
	make -C apps/linktest clean
	make -C apps/multiprog clean
	make -C apps/sync clean
	make -C apps/POLite/heat-gals clean
	make -C apps/POLite/heat-sync clean
	make -C apps/POLite/asp-gals clean
	make -C apps/POLite/asp-sync clean
	make -C apps/POLite/asp-pc clean
	make -C apps/POLite/pagerank-sync clean
	make -C apps/POLite/pagerank-gals clean
	make -C bin clean
	make -C tests clean
