// SPDX-License-Identifier: BSD-2-Clause
#ifndef _GRID_H_
#define _GRID_H_

#include <tinsel.h>
#include "boxes.h"

// Direction
typedef enum {N, S, E, W} Dir;

// Opposite direction
INLINE Dir opposite(Dir d)
{
  if (d == N) return S;
  if (d == S) return N;
  if (d == E) return W;
  return E;
}

INLINE int getWidth() { return 96 * USE_BOXES_X; }
INLINE int getHeight() { return 64 * USE_BOXES_Y; }

// Get X coord of thread in grid
INLINE int getX(int addr)
{
  uint32_t boardX, boardY, tileX, tileY, coreId, threadId;
  tinselFromAddr(addr, &boardX, &boardY, &tileX, &tileY, &coreId, &threadId);
  uint32_t x = boardX;
  x = (x << TinselMailboxMeshXBits) | tileX;
  x = (x << 1) | (coreId & 1);
  x = (x << 2) | (threadId & 3);
  return x;
}

// Get Y coord of thread in grid
INLINE int getY(int addr)
{
  uint32_t boardX, boardY, tileX, tileY, coreId, threadId;
  tinselFromAddr(addr, &boardX, &boardY, &tileX, &tileY, &coreId, &threadId);
  uint32_t y = boardY;
  y = (y << TinselMailboxMeshYBits) | tileY;
  y = (y << 1) | (coreId>>1);
  y = (y << 2) | (threadId>>2);
  return y;
}

// Determine thread id from X and Y coords
INLINE int fromXY(int x, int y)
{
  uint32_t boardX, boardY, tileX, tileY, coreId, threadId;
  threadId = (x&3) | ((y&3) << 2);
  x >>= 2; y >>= 2;
  coreId = (x&1) | ((y&1) << 1);
  x >>= 1; y >>= 1;
  tileX = x & ((1<<TinselMailboxMeshXBits)-1);
  x >>= TinselMailboxMeshXBits;
  boardX = x;
  tileY = y & ((1<<TinselMailboxMeshYBits)-1);
  y >>= TinselMailboxMeshYBits;
  boardY = y;
  return tinselToAddr(
           boardX, boardY,
             tileX, tileY,
               coreId, threadId);
}

// North neighbour
INLINE int north()
{
  int me = tinselId();
  int x  = getX(me);
  int y  = getY(me);
  return y == 0 ? -1 : fromXY(x, y-1);
}

// South neighbour
INLINE int south()
{
  int me = tinselId();
  int x  = getX(me);
  int y  = getY(me);
  return y == (64*USE_BOXES_Y)-1 ? -1 : fromXY(x, y+1);
}

// East neighbour
INLINE int east()
{
  int me = tinselId();
  int x  = getX(me);
  int y  = getY(me);
  return x == (96*USE_BOXES_X)-1 ? -1 : fromXY(x+1, y);
}

// West neighbour
INLINE int west()
{
  int me = tinselId();
  int x  = getX(me);
  int y  = getY(me);
  return x == 0 ? -1 : fromXY(x-1, y);
}

// Emit colour for executing thread
INLINE void emit(int col)
{
  volatile int* flit = (volatile int*) tinselSlot(15);
  int me = tinselId();
  flit[0] = getX(me);
  flit[1] = getY(me);
  flit[2] = col;
  tinselWaitUntil(TINSEL_CAN_SEND);
  tinselSend(tinselHostId(), flit);
}

#endif
