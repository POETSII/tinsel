#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Grab an unsigned int from stdin
uint32_t getUInt32()
{
  uint32_t val;
  if (scanf("%u", &val) <= 0) {
    fprintf(stderr, "TraceGen.c: parse error\n");
    exit(EXIT_FAILURE);
  }
  return val;
}
