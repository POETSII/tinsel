// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor

// Routines for simulating mixed-width multi-port block Rams.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

// Create
uint64_t createBlockRam(uint32_t sizeInBits)
{
  int8_t* ram = (int8_t*) malloc(sizeInBits);
  for (uint32_t i = 0; i < sizeInBits; i++) ram[i] = 0;
  return (uint64_t) ram;
}

// Write
void blockRamWrite(
       uint64_t handle,
       uint32_t* addr,
       uint32_t* data,
       uint32_t dataWidth,
       uint32_t addrWidth)
{
  assert(addrWidth < 32);
  int8_t* ram = (int8_t*) handle;
  int base = *addr * dataWidth;
  int bitCount = 0;
  for (int i = 0; i < dataWidth; i++) {
    ram[base+i] = *data & 1;
    *data >>= 1;
    bitCount++;
    if (bitCount == 32) {
      bitCount = 0;
      data++;
    }
  }
}

// Read
void blockRamRead(
       uint32_t* data,
       uint64_t handle,
       uint32_t* addr,
       uint32_t dataWidth,
       uint32_t addrWidth)
{
  assert(addrWidth < 32);
  int8_t* ram = (int8_t*) handle;
  int base = *addr * dataWidth;
  int bitCount = 0;
  *data = 0;
  for (int i = dataWidth-1; i >= 0; i--) {
    *data = (*data << 1) | ram[base+i];
    bitCount++;
    if (bitCount == 32) {
      bitCount = 0;
      data++;
      *data = 0;
    }
  }
}
