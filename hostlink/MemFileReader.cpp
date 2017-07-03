// Memory files in "verilog" format, produced by objcopy, have the
// following structure:
//
//   @00000200
//   6F 00 40 00 13 01
//
//   @00100000
//   48 65 6C 6C 6F 20 66 72 6F 6D 20 74 68 72 65 61 
//   64 20 30 78 25 78 0A 00 
//
// The @ sign denotes a start address.  The hex bytes that follow it,
// up to the next @ sign, are a contiguous stream of bytes starting
// at that address.

#include "MemFileReader.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

// Constructor
MemFileReader::MemFileReader(const char* filename)
{
  address = 0;
  fp = fopen(filename, "rt");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open file '%s'\n", filename);
    exit(EXIT_FAILURE);
  }
}

// Read a byte
bool MemFileReader::getByte(uint32_t* addr, uint8_t* byte)
{
  assert(fp != NULL);
  uint32_t data;
  *addr = address;
  while (fscanf(fp, " @%x", &address) > 0) *addr = address;
  if (fscanf(fp, " %x", &data) > 0) {
    *byte = (uint8_t) data;
    address++;
    return true;
  }
  return false;
}

// Read a 32-bit word
bool MemFileReader::getWord(uint32_t* addr, uint32_t* word)
{
  assert(fp != NULL);
  *word = 0;
  uint8_t* bytePtr = (uint8_t*) word;
  uint32_t data;
  *addr = address;
  int count = 0;
  while (fscanf(fp, " @%x", &address) > 0) *addr = address;
  while (fscanf(fp, " %x", &data) > 0) {
    bytePtr[count] = (uint8_t) data;
    count++;
    if (count == 4) break;
  }
  if (count != 0) {
    address += 4;
    return true;
  }
  return false;
}

// Destructor
MemFileReader::~MemFileReader()
{
  if (fp != NULL) fclose(fp);
}
