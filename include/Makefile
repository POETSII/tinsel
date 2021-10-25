CONFIG=../config.py

config.h: $(CONFIG)
	echo "#ifndef _CONFIG_H_" > config.h
	echo "#define _CONFIG_H_" >> config.h
	python3 $(CONFIG) cpp >> config.h
	echo "#endif" >> config.h

.PHONY: clean
clean:
	rm -f config.h
