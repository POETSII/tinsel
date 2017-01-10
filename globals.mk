# Common directories
INC = $(realpath $(TINSEL_ROOT)/include)
QP  = $(realpath $(TINSEL_ROOT)/de5)
RTL = $(realpath $(TINSEL_ROOT)/rtl)
BIN = $(realpath $(TINSEL_ROOT)/bin)

# RISC-V tools
RV_ARCH    = RV32IM
RV_CC      = riscv64-unknown-elf-gcc
RV_LD      = riscv64-unknown-elf-ld
RV_OBJCOPY = riscv64-unknown-elf-objcopy

# Extend PATH
export PATH := $(PATH):$(realpath $(BIN))

# Set path to config script
export CONFIG := $(realpath $(TINSEL_ROOT)/config.py)
