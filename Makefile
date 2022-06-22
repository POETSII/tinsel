# SPDX-License-Identifier: BSD-2-Clause

.PHONY: nothing
nothing:

.PHONY: clean
clean:
	make -C rtl clean
	-make -C de10-pro clean
