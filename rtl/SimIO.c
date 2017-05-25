#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

// Grab an unsigned int from stdin
uint32_t getUInt32()
{
  uint32_t val;
  if (scanf("%i", &val) <= 0) {
    fprintf(stderr, "TraceGen.c: getUInt32() parse error\n");
    exit(EXIT_FAILURE);
  }
  return val;
}

// Grab a char from stdin
uint8_t getChar()
{
  int i;
  while (1) {
    i = getchar();
    if (i == EOF) {
      fprintf(stderr, "TraceGen.c: getChar() parse error\n");
      exit(EXIT_FAILURE);
    }
    if (! isspace(i)) return (uint8_t) i;
  }
}
