// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>
#include <assert.h>
#include "linkrate.h"

void measureLink(
  HostLink* hostLink,
  uint32_t srcX, uint32_t srcY,
  uint32_t dstX, uint32_t dstY)
{
  assert(srcX <= 7 && srcY <= 7);
  assert(dstX <= 7 && dstY <= 7);

  uint32_t srcVal = (srcY << 5) | (srcX << 2) | 1;
  uint32_t dstVal = (dstY << 5) | (dstX << 2) | 1;

  uint32_t boardsX = BOXES_X * TinselMeshXLenWithinBox;
  uint32_t boardsY = BOXES_Y * TinselMeshYLenWithinBox;
  for (uint32_t t = 0; t < TinselThreadsPerCore; t++) {
    for (uint32_t y = 0; y < boardsY; y++) {
      for (uint32_t x = 0; x < boardsX; x++) {
        if (x == srcX && y == srcY) {
          hostLink->debugLink->setBroadcastDest(x, y, t);
          hostLink->debugLink->put(srcX, srcY, dstVal);
        }
        else if (x == dstX && y == dstY) {
          hostLink->debugLink->setBroadcastDest(x, y, t);
          hostLink->debugLink->put(dstX, dstY, srcVal);
        }
        else {
          hostLink->debugLink->setBroadcastDest(x, y, t);
          hostLink->debugLink->put(x, y, 0);
        }
      }
    }
  }

  // Wait for one response each from source and destination
  uint32_t buffer[1 << TinselLogWordsPerMsg];
  for (uint32_t i = 0; i < 2; i++) {
    hostLink->recv(buffer);
    if (i == 0) {
      double duration = (double) buffer[0];
      double rate = (24.0*NumMsgs*TinselThreadsPerBoard) / (duration/MHZ);
      printf("(%d, %d) (%d, %d): %.1lf MB/s\n",
        srcX, srcY, dstX, dstY, rate);
    }
  }
}

void measureLinks(HostLink* hostLink)
{
  uint32_t boardsX = BOXES_X * TinselMeshXLenWithinBox;
  uint32_t boardsY = BOXES_Y * TinselMeshYLenWithinBox;

  // Iterate over horizontal links
  for (uint32_t y = 0; y < boardsY; y++) {
    for (uint32_t x = 0; x < boardsX-1; x++) {
      measureLink(hostLink, x, y, x+1, y);
    }
  }

  // Iterate over vertical links
  for (uint32_t x = 0; x < boardsX; x++) {
    for (uint32_t y = 0; y < boardsY-1; y++) {
      measureLink(hostLink, x, y, x, y+1);
    }
  }
}

int main()
{
  HostLink hostLink(BOXES_X, BOXES_Y);

  hostLink.boot("code.v", "data.v");

  printf("Starting\n");
  hostLink.go();

  measureLinks(&hostLink);

  return 0;
}
