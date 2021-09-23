# SPDX-License-Identifier: BSD-2-Clause

# Common directories
INC = $(realpath $(TINSEL_ROOT)/include)
LIB = $(realpath $(TINSEL_ROOT)/lib)

STRATIX10 = $(shell python $(TINSEL_ROOT)/config.py print Stratix10)
STRATIXV = $(shell python $(TINSEL_ROOT)/config.py print StratixV)

ifeq ($(STRATIX10),True)
QP  = $(realpath $(TINSEL_ROOT)/DE10Pro/DE10-reference-project/quartus)
endif

ifeq ($(STRATIXV),True)
QP  = $(realpath $(TINSEL_ROOT)/de5)
endif

RTL = $(realpath $(TINSEL_ROOT)/rtl)
BIN = $(realpath $(TINSEL_ROOT)/bin)
HL  = $(realpath $(TINSEL_ROOT)/hostlink)

# RISC-V tools
RV_ARCH     = rv32imf
RV_CC       = riscv64-unknown-elf-gcc
RV_CPPC     = riscv64-unknown-elf-g++
RV_LD       = riscv64-unknown-elf-ld
RV_OBJCOPY  = riscv64-unknown-elf-objcopy
RV_CFLAGS   = -mabi=ilp32 -march=$(RV_ARCH) -static -mcmodel=medany \
              -fvisibility=hidden -nostdlib -nostartfiles \
              -fsingle-precision-constant -fno-builtin-printf \
              -ffp-contract=off -fno-builtin -ffreestanding

# Extend PATH
export PATH := $(PATH):$(realpath $(BIN))

# Set path to config script
export CONFIG := $(realpath $(TINSEL_ROOT)/config.py)
