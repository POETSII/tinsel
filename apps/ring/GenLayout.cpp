#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <config.h>
#include "ring.h"

void swap(int &x, int&y) {
  int tmp = x;
  x = y;
  y = tmp;
}

int main()
{
  int numThreads = (1 << TinselLogThreadsPerBoard)
                     * TinselMeshXLen * TinselMeshYLen;
  int numThreadsUsed = (1 << LOG_THREADS_PER_BOARD) * NUM_BOARDS;

  // This sequence maps each thread id to its neighbour
  // (-1 denotes thread has no neighbour and is unused)
  int neighbour[numThreads];

  if (RANDOM) {
    // Sequence of thread ids with no duplicates
    int seq[numThreads];

    // Generate and shuffle
    for (int i = 0; i < numThreads; i++) seq[i] = i;
    srand(0);
    for (int i = 1; i < numThreads-1; i++) {
      int j = i + rand() % (numThreads-i);
      swap(seq[i], seq[j]);
    }

    // Determine neighbours
    for (int i = 0; i < numThreads; i++) {
      if (i < numThreadsUsed)
        neighbour[seq[i]] = seq[(i+1) % numThreadsUsed];
      else
        neighbour[seq[i]] = -1;
    }
  }
  else {
    // Increment to get to the next thread in ring
    uint32_t threadIncr = 1 << (TinselLogThreadsPerBoard-LOG_THREADS_PER_BOARD);

    for (int i = 0; i < numThreads; i++) {
      // Next thread in ring
      uint32_t next = (i+threadIncr) >= numThreads ? 0 :
        i+(1 << (TinselLogThreadsPerBoard - LOG_THREADS_PER_BOARD));

      if ((i & (threadIncr-1)) == 0)
        neighbour[i] = next;
      else
        neighbour[i] = -1;
    }
  }

  printf("int layout[%d] = {\n", numThreads);
  for (int i = 0; i < numThreads; i++) {
    printf("%i", neighbour[i]);
    if ((i+1) < numThreads) printf(",");
    if ((i%10) == 9) printf("\n");
  }
  printf("};\n");

  return 0;
}
