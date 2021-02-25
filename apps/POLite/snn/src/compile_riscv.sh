#!/bin/bash

set -euo pipefail

if [[ "$TINSEL_ROOT" == "" ]] ; then
    TINSEL_ROOT=../../../..
fi

TINSEL_ROOT=$(realpath ${TINSEL_ROOT})
echo "TINSEL_ROOT=${TINSEL_ROOT}"

INPUT_PATH=$(realpath "$1")
if [[ ! -f "${INPUT_PATH}" ]] ; then
    >&2 echo "Source file ${INPUT_PATH} does not exist."
    exit 1
fi

if [[ $# -lt 2 ]] ; then
    >&2 echo "Need to specify working directory where code.v and data.v go."
    exit 1
fi

WD="$2"
if [[ ! -d ${WD} ]] ; then
    >&2 echo "Creating dest directory ${WD}"
    mkdir -p ${WD}
fi
WD=$(realpath "$2")

mkdir -p ${WD}

RV_ARCH=rv32imf
RV_CC=riscv64-unknown-elf-gcc
RV_CXX=riscv64-unknown-elf-g++
RV_LD=riscv64-unknown-elf-ld
RV_OBJCOPY=riscv64-unknown-elf-objcopy
RV_CFLAGS=" -mabi=ilp32 -march=${RV_ARCH} -static -mcmodel=medany
              -fvisibility=hidden -nostdlib -nostartfiles
              -fsingle-precision-constant -fno-builtin-printf
              -ffp-contract=off -fno-builtin -ffreestanding"

CFLAGS="${RV_CFLAGS} -DTINSEL=1 -DNDEBUG=1 -I ${TINSEL_ROOT}/include -I ${TINSEL_ROOT}/apps/POLite/snn/include"
CFLAGS="${CFLAGS} -I ${TINSEL_ROOT}/hostlink -Os -g"

LDFLAGS="-Wl,-melf32lriscv -Wl,-G0 "

${TINSEL_ROOT}/apps/POLite/util/genld.sh > ${WD}/link.ld

echo "#include \"${TINSEL_ROOT}/lib/io.c\"" > $WD/unity.cpp
echo "#include \"${INPUT_PATH}\"" >> $WD/unity.cpp

>&2 echo "Compiling obj"
${RV_CXX} -c ${CFLAGS} -fwhole-program -o $WD/unity.o $WD/unity.cpp
>&2 echo "Linking"
${RV_CXX} ${CFLAGS} ${LDFLAGS} -T $WD/link.ld -o $WD/app.elf \
    ${TINSEL_ROOT}/apps/POLite/util/entry.S $WD/unity.o src/memmove.cpp

>&2 echo "Checking elf"
${TINSEL_ROOT}/bin/checkelf.sh $WD/app.elf

>&2 echo "Extracting code"
${RV_OBJCOPY} -O verilog --only-section=.text ${WD}/app.elf ${WD}/code.v

>&2 echo "Extracting data"
${RV_OBJCOPY} -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents ${WD}/app.elf ${WD}/data.v