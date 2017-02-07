#ifndef _HOSTLINK_H_
#define _HOSTLINK_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "RawLink.h"

#define HOSTLINK_SET_DEST 0
#define HOSTLINK_STD_IN   1
#define HOSTLINK_STD_OUT  2

class HostLink {
  RawLink raw;
 public:

  inline void setDest(uint32_t dest) {
    uint8_t cmd = HOSTLINK_SET_DEST;
    raw.put(&cmd, 1);
    raw.put(&dest, 4);
  }

  inline void put(uint32_t word) {
    uint8_t cmd = HOSTLINK_STD_IN;
    raw.put(&cmd, 1);
    raw.put(&word, 4);
  }

  inline uint8_t get(uint32_t *src, uint32_t* word) {
    uint8_t cmd;
    raw.get(&cmd, 1);
    raw.get(src, 4);
    raw.get(word, 4);
    return cmd;
  }

  inline bool canGet() {
    return raw.canGet();
  }
};

#endif
