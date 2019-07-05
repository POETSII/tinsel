// Functions for working with a 2D grid of threads

#ifndef _GRID_2D_H_
#define _GRID_2D_H_

#include <stdint.h>
#include <tinsel-interface.h>

// Determine dimensions for a rectangle of threads within a tile
INLINE void gridTileDims(uint32_t* logWidth, uint32_t* logHeight)
{
  *logWidth = TinselLogCoresPerMailbox;
  *logHeight = TinselLogThreadsPerCore;
  while (*logWidth != *logHeight) {
    uint32_t w = *logWidth + 1;
    uint32_t h = *logHeight - 1;
    if (w > h) return;
    *logWidth = w;
    *logHeight = h;
  }
}

// Determine grid dimensions, given number of boxes to use
INLINE void gridDims(
  uint32_t numBoxesX,
  uint32_t numBoxesY,
  uint32_t* gridWidth,
  uint32_t* gridHeight)
{
  uint32_t tw, th;
  gridTileDims(&tw, &th);
  tw = 1 << tw;
  th = 1 << th;
  *gridWidth = tw * TinselMailboxMeshXLen *
    TinselMeshXLenWithinBox * numBoxesX;
  *gridHeight = th * TinselMailboxMeshYLen *
    TinselMeshYLenWithinBox * numBoxesY;
}

// Convert thread id to grid position
INLINE void gridToPos(uint32_t addr, uint32_t* x, uint32_t* y)
{
   // Determine location
  uint32_t boardX, boardY, tileX, tileY, coreId, threadId;
  tinselFromAddr(addr, &boardX, &boardY,
                       &tileX, &tileY,
                       &coreId, &threadId);
  uint32_t localId = coreId * TinselThreadsPerCore + threadId;

   // Determine coords within tile
  uint32_t logTileWidth;
  uint32_t logTileHeight;
  gridTileDims(&logTileWidth, &logTileHeight);
  uint32_t localX = localId & ((1<<logTileWidth)-1);
  uint32_t localY = localId >> logTileWidth;
  uint32_t tileWidth = 1 << logTileWidth;
  uint32_t tileHeight = 1 << logTileHeight;

  // Determine grid coords
  *x = boardX * TinselMailboxMeshXLen * tileWidth +
         tileX * tileWidth + localX;
  *y = boardY * TinselMailboxMeshYLen * tileHeight +
         tileY * tileHeight + localY;
}

// Software divider
INLINE void div(uint32_t n, uint32_t d, uint32_t* q, uint32_t* r)
{
  uint32_t x = 0; // Quotient
  int32_t mag = 0;
  while (d < n && (int32_t) d >= 0) { d <<= 1; mag++; }
  while (mag >= 0) {
    if (d <= n) {
      n -= d;
      x += 1 << mag;
    }
    mag--;
    d >>= 1;
  }
  *q = x;
  *r = n;
}

// Convert grid position to thread id
INLINE uint32_t gridFromPos(uint32_t x, uint32_t y)
{
  uint32_t logTileWidth;
  uint32_t logTileHeight;
  gridTileDims(&logTileWidth, &logTileHeight);

  // Determine X fields
  uint32_t localX = x & ((1<<logTileWidth)-1);
  x >>= logTileWidth;
  uint32_t tileX, boardX;
  div(x, TinselMailboxMeshXLen, &boardX, &tileX);

  // Determine Y fields
  uint32_t localY = y & ((1<<logTileHeight)-1);
  y >>= logTileHeight;
  uint32_t tileY, boardY;
  div(y, TinselMailboxMeshYLen, &boardY, &tileY);
 
  // Determine core and thread ids
  uint32_t localId = localX | (localY << logTileWidth);
  uint32_t threadId = localId & ((1 << TinselLogThreadsPerCore)-1);
  uint32_t coreId = localId >> TinselLogThreadsPerCore;

  return tinselToAddr(boardX, boardY,
           tileX, tileY, coreId, threadId);
}

// Determine neighbour relative to given thread
INLINE int32_t gridNeighbour(uint32_t addr,
                 uint32_t gridWidth, uint32_t gridHeight,
                   int32_t xOffset, int32_t yOffset)
{
  int32_t x, y;
  gridToPos(addr, (uint32_t*) &x, (uint32_t*) &y);
  if (x+xOffset < 0) return -1;
  if (y+yOffset < 0) return -1;
  if (x+xOffset >= (int32_t) gridWidth) return -1;
  if (y+yOffset >= (int32_t) gridHeight) return -1;
  return gridFromPos(x+xOffset, y+yOffset);
}

#endif
