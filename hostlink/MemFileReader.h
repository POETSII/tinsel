#ifndef _MEMFILEREADER_H_
#define _MEMFILEREADER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

class MemFileReader {
  FILE* fp;
  uint32_t address;

 public:
  // Constructor
  MemFileReader(const char* filename);

  // Read a byte
  bool getByte(uint32_t* addr, uint8_t* byte);

  // Read a 32-bit word
  bool getWord(uint32_t* addr, uint32_t* word);

  // Destructor
  ~MemFileReader();
};

#endif
