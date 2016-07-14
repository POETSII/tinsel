// Copyright (c) Matthew Naylor

// This module provides 4GB of RAM to BlueSim for simulating DRAM.  It
// divides the 4GB into 1M x 4KB pages, and allocates pages on demand.
// The initial contents is all zeroes.  It provides functions to read
// and write 32-bit words.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Globals
uint32_t** ram = NULL;

// Initialise
void ramInit()
{
  int i;
  ram = (uint32_t**) malloc((1<<20) * sizeof(uint32_t*));
  for (i = 0; i < (1<<20); i++) ram[i] = NULL;
}

// Write
void ramWrite(uint32_t addr, uint32_t data)
{
  uint32_t page = addr >> 12;
  uint32_t offset = (addr & 0xfff) >> 2;
  int i;
  if (ram == NULL) ramInit();
  if (ram[page] == NULL) {
    ram[page] = (uint32_t*) malloc((1<<10) * sizeof(uint32_t));
    for (i = 0; i < (1<<10); i++) ram[page][i] = 0;
  }
  ram[page][offset] = data;
}

// Read
uint32_t ramRead(uint32_t addr)
{
  uint32_t page = addr >> 12;
  uint32_t offset = (addr & 0xfff) >> 2;
  if (ram == NULL) ramInit();
  if (ram[page] == NULL) return 0;
  return ram[page][offset];
}
