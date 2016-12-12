#!/bin/bash

echo -e '#ifndef _CONFIG_H_' > config.h
echo -e '#define _CONFIG_H_\n' >> config.h
python ../config.py cpp >> config.h
echo -e '\n#endif' >> config.h
