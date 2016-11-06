// Copyright (c) Matthew Naylor

// This module provides 4GB of RAM to BlueSim for simulating DRAM.  It
// divides the 4GB into 1M x 4KB pages, and allocates pages on demand.
// The initial contents is all zeroes.  It provides functions to read
// and write 32-bit words.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define MAX_DRAMS_PER_BOARD 8

// Globals
uint32_t** ram[MAX_DRAMS_PER_BOARD] =
  {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

// Initialise
void ramInit(uint8_t ramId)
{
  int i;
  assert(ramId < MAX_DRAMS_PER_BOARD);
  ram[ramId] = (uint32_t**) malloc((1<<20) * sizeof(uint32_t*));
  for (i = 0; i < (1<<20); i++) ram[ramId][i] = NULL;
}

// Write
void ramWrite(uint8_t ramId, uint32_t addr, uint32_t data, uint32_t bitEn)
{
  uint32_t page = addr >> 12;
  uint32_t offset = (addr & 0xfff) >> 2;
  int i;
  assert(ramId < MAX_DRAMS_PER_BOARD);
  if (ram[ramId] == NULL) ramInit(ramId);
  if (ram[ramId][page] == NULL) {
    ram[ramId][page] = (uint32_t*) malloc((1<<10) * sizeof(uint32_t));
    for (i = 0; i < (1<<10); i++) ram[ramId][page][i] = 0;
  }
  uint32_t val = ram[ramId][page][offset];
  ram[ramId][page][offset] = (data & bitEn) | (val & ~bitEn);
}

// Read
uint32_t ramRead(uint8_t ramId, uint32_t addr)
{
  uint32_t page = addr >> 12;
  uint32_t offset = (addr & 0xfff) >> 2;
  assert(ramId < MAX_DRAMS_PER_BOARD);
  if (ram[ramId] == NULL) ramInit(ramId);
  if (ram[ramId][page] == NULL) return 0;
  return ram[ramId][page][offset];
}
