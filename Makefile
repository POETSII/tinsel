.PHONY: nothing
nothing:

.PHONY: clean
clean:
	make -C rtl clean
	make -C de5 clean
	make -C tools/hostlink clean
	make -C include clean
	make -C apps/hello clean
	make -C apps/boot clean
