CONFIG=../config.py

# dt10 : There is a race condition here if you do parallel builds,
# as this rule seems to get called from lots of places via recursive
# makes that arent in the global make dependency graph.
config.h: $(CONFIG)
	T=$(mktemp) ; \
	echo "#ifndef _CONFIG_H_" > $(T) ; \
	echo "#define _CONFIG_H_" >> $(T) ; \
	python $(CONFIG) cpp >> $(T) ; \
	echo "#endif" >> $(T) ; \
	mv -f $(T) config.h

.PHONY: clean
clean:
	rm -f config.h
