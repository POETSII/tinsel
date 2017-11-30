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
	make -C apps/linktest clean
	make -C apps/POLite/heat clean
	make -C apps/POLite/asp clean
	make -C apps/POLite/sorter clean
	make -C apps/Synch/heat clean
	make -C apps/Synch/asp clean
	make -C apps/Synch/sorter clean
	make -C bin clean
	make -C tests clean
