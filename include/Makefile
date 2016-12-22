CONFIG=../config.py

config.h: $(CONFIG)
	echo '#ifndef _CONFIG_H_' > config.h
	echo '#define _CONFIG_H_\n' >> config.h
	python $(CONFIG) cpp >> config.h
	echo '\n#endif' >> config.h

.PHONY: clean
clean:
	rm -f config.h
