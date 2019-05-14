// SPDX-License-Identifier: BSD-2-Clause
#ifndef _RANDOM_SET_H_
#define _RANDOM_SET_H_

// Generate a random set
inline void randomSet(uint32_t n, uint32_t* set, uint32_t max)
{
  srand(0);
  for (int i = 0; i < n; i++) {
    retry:
    set[i] = rand() % max;
    for (int j = 0; j < i; j++)
      if (set[i] == set[j]) goto retry;
  }
}

#endif
