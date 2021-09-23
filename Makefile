# SPDX-License-Identifier: BSD-2-Clause

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
	make -C apps/custom clean
	make -C apps/ring clean
	make -C apps/linkrate clean
	make -C apps/multiprog clean
	make -C apps/sync clean
	make -C apps/temps clean
	make -C apps/POLite/heat-gals clean
	make -C apps/POLite/heat-sync clean
	make -C apps/POLite/heat-cube-sync clean
	make -C apps/POLite/heat-grid-sync clean
	make -C apps/POLite/asp-gals clean
	make -C apps/POLite/asp-sync clean
	make -C apps/POLite/pagerank-sync clean
	make -C apps/POLite/pagerank-gals clean
	make -C apps/POLite/sssp-sync clean
	make -C apps/POLite/sssp-async clean
	make -C apps/POLite/clocktree-async clean
	make -C apps/POLite/izhikevich-gals clean
	make -C apps/POLite/izhikevich-sync clean
	make -C apps/POLite/pressure-sync clean
	make -C apps/POLite/hashmin-sync clean
	make -C apps/POLite/progrouters clean
	make -C bin clean
	make -C tests clean
	make -C DE10Pro/DE10-reference-project XXCLEAN

DE10: clean
	make -C rtl mkDE10Top.v
	make -C apps/boot all
	make -C DE10Pro/DE10-reference-project build/top.sof
