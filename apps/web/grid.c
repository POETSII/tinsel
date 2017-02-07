#include <tinsel.h>
#include <grid.h>

// Given a direction, return opposite direction
Dir opposite(Dir d)
{
  if (d == N) return S;
  if (d == S) return N;
  if (d == E) return W;
  return E;
}

// Get X coord of thread in grid
int getX(int threadId)
{
  // Length of square grid of threads
  const uint32_t logLen = TinselLogThreadsPerBoard >> 1;
  const uint32_t len    = 1 << logLen;
  // Return X coord
  return threadId & (len-1);
}

// Get Y coord of thread in grid
int getY(int threadId)
{
  // Length of square grid of threads
  const uint32_t logLen = TinselLogThreadsPerBoard >> 1;
  // Return Y coord
  return threadId >> logLen;
}

// Determine thread id from X and Y coords
int fromXY(int x, int y)
{
  const uint32_t logLen = TinselLogThreadsPerBoard >> 1;
  return (y << logLen) | x;
}

// North neighbour
int north()
{
  int me = tinselId();
  int x  = getX(me);
  int y  = getY(me);
  return y == 0 ? -1 : fromXY(x, y-1);
}

// South neighbour
int south()
{
  const uint32_t logLen = TinselLogThreadsPerBoard >> 1;
  const uint32_t len    = 1 << logLen;
  int me = tinselId();
  int x  = getX(me);
  int y  = getY(me);
  return y == len-1 ? -1 : fromXY(x, y+1);
}

// East neighbour
int east()
{
  const uint32_t logLen = TinselLogThreadsPerBoard >> 1;
  const uint32_t len    = 1 << logLen;
  int me = tinselId();
  int x  = getX(me);
  int y  = getY(me);
  return x == len-1 ? -1 : fromXY(x+1, y);
}

// West neighbour
int west()
{
  int me = tinselId();
  int x  = getX(me);
  int y  = getY(me);
  return x == 0 ? -1 : fromXY(x-1, y);
}

// Emit colour for executing thread
void emit(int col)
{
  int me = tinselId();
  int x  = getX(me);
  int y  = getY(me);
  // Transfer Y coord (12 bits), X coord (12 bits) and colour (8 bits)
  uint32_t coords = (y << 12) | x;
  tinselHostPut((coords << 8) | (col & 0xff));
}
