#ifndef _BITMAP_H_
#define _BITMAP_H_

#include <stdint.h>
#include <assert.h>
#include <POLite/Seq.h>

struct Bitmap {
  // Bitmap contents (sequence of 64-bit words)
  Seq<uint64_t>* contents;

  // Index of first non-full word in bitmap
  uint32_t firstFree;

  // Constructor
  Bitmap() {
    contents = new Seq<uint64_t> (16);
    firstFree = 0;
  }

  // Destructor
  ~Bitmap() {
    if (contents) delete contents;
  }

  // Get value of word at given index, return 0 if out-of-bounds
  inline uint64_t getWord(uint32_t index) {
    return index >= contents->numElems ? 0ul : contents->elems[index];
  }

  // Find index of next free word in bitmap starting from given word index
  inline uint32_t nextFreeWordFrom(uint32_t start) {
    for (uint32_t i = start; i < contents->numElems; i++)
      if (~contents->elems[i] != 0ul) return i;
    return contents->numElems;
  }

  // Set bit at given index and bit offset in bitmap
  inline void setBit(uint32_t wordIndex, uint32_t bitIndex) {
    for (uint32_t i = contents->numElems; i <= wordIndex; i++)
      contents->append(0ul);
    contents->elems[wordIndex] |= 1ul << bitIndex;
    if (wordIndex == firstFree) {
      firstFree = nextFreeWordFrom(firstFree);
    }
  }

  // Find index of next zero bit, and flip that bit
  inline uint32_t grabNextBit() {
    uint64_t word = ~getWord(firstFree);
    assert(word != 0ul);
    uint32_t bit = __builtin_ctzll(word);
    setBit(firstFree, bit);
    return 64*firstFree + bit;
  }
};

#endif
