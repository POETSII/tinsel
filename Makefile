.PHONY: nothing
nothing:

.PHONY: clean
clean:
	make -C rtl clean
	make -C de5 clean
	make -C de5/host-board clean
	make -C hostlink clean
	make -C include clean
	make -C lib clean
	make -C apps/hello clean
	make -C apps/boot clean
	make -C apps/heat clean
	make -C apps/ping clean
	make -C bin clean
	make -C tests clean
