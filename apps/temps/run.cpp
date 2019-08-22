// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

#define NUM_BOXES_X 1
#define NUM_BOXES_Y 1

int main()
{
  HostLink hostLink(NUM_BOXES_X, NUM_BOXES_Y);

  for (int32_t y = 0; y < NUM_BOXES_Y; y++) {
    for (int32_t x = 0; x < NUM_BOXES_X; x++) {
      int32_t temp = hostLink.debugLink->getBridgeTemp(x, y);
      printf("Bridge(%i, %i): %i\n", x, y, temp);
    }
  }

  int32_t numBoardsX = NUM_BOXES_X * TinselMeshXLenWithinBox;
  int32_t numBoardsY = NUM_BOXES_Y * TinselMeshYLenWithinBox;
  for (int32_t y = 0; y < numBoardsY; y++) {
    for (int32_t x = 0; x < numBoardsX; x++) {
      int32_t temp = hostLink.debugLink->getBoardTemp(x, y);
      printf("Board(%i, %i): %i\n", x, y, temp);
    }
  }

  return 0;
}
