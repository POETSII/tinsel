// SPDX-License-Identifier: BSD-2-Clause
// Simulate heat diffusion on 2D grid of threads.
// Each thread handles an 8x8 subgrid of cells.

#include <tinsel.h>
#include <grid2d.h>
#include "heat.h"

// 32-bit fixed-point number in 16.16 format
#define FixedPoint(x, y) (((x) << 16) | (y))

// Poles
// -----

// Direction: north, south, east, and west
typedef enum {N=0, S=1, E=2, W=3} Dir;

// Given a direction, return opposite direction
inline Dir opposite(Dir d) { return d^1; }

// Message format
// --------------

typedef struct {
  // The time step that the sender is on
  int time;
  // Which direction did the message come from?
  Dir from;
  // Temperatures from the sender
  int temp[8];
} Msg;

// Output
// ------

// Emit the state of the thread-local subgrid
void emitGrid(int (*subgrid)[8])
{
  // Messages will be comprised of 1 flit
  tinselSetLen(0);
  // Determine X and Y position of thread
  uint32_t xPos, yPos;
  gridToPos(tinselId(), &xPos, &yPos);
  // Determine X and Y pixel coordinates
  uint32_t x = xPos * 8;
  uint32_t y = yPos * 8;
  // Host id
  uint32_t hostId = tinselHostId();
  // Message to be sent to the host
  volatile HostMsg* msg = tinselSlot(7);
  // Send subgrid to host
  for (int i = 0; i < 8; i++) {
    tinselWaitUntil(TINSEL_CAN_SEND);
    msg->x = x;
    msg->y = y+i;
    for (int j = 0; j < 8; j++)
      msg->temps[j] = (subgrid[i][j] >> 16);
    tinselSend(hostId, msg);
  }
  // Restore message size of 3 flits
  tinselSetLen(2);
}

// Top-level
// ---------

int main()
{
  // Subgrid memory
  // --------------

  // Space for two subgrids (the current state and the next state)
  int subgridSpace[8][8];
  int newSubgridSpace[8][8];

  // Pointers to subgrids, used in a double-buffer fashion
  int (*subgrid)[8] = subgridSpace;
  int (*newSubgrid)[8] = newSubgridSpace;
  int (*subgridPtr)[8];

  // Subgrid location
  // ----------------

  uint32_t xPos, yPos;
  gridToPos(tinselId(), &xPos, &yPos);

  uint32_t xLen, yLen;
  gridDims(X_BOXES, Y_BOXES, &xLen, &yLen);

  // Neighbours
  // ----------

  // The thread to the north, south, east, and west
  int neighbour[4];
  neighbour[N] = yPos == 0      ? -1 : gridFromPos(xPos, yPos-1);
  neighbour[S] = yPos == yLen-1 ? -1 : gridFromPos(xPos, yPos+1);
  neighbour[E] = xPos == xLen-1 ? -1 : gridFromPos(xPos+1, yPos);
  neighbour[W] = xPos == 0      ? -1 : gridFromPos(xPos-1, yPos);

  // List containing each neighbour
  // (Only the first numNeighbours elements are valid)
  Dir neighbourList[4];
  int numNeighbours = 0;
  for (int i = 0; i < 4; i++)
    if (neighbour[i] >= 0) neighbourList[numNeighbours++] = i;

  // Initial state
  // -------------

  // Messages to be sent to neighbours
  volatile Msg* msgOut[4];
  for (int i = 0; i < 4; i++) msgOut[i] = tinselSlot(i);

  // Messages received from neighbours
  Msg msgIn[4];

  // Buffer for messages received from neighbours
  // (At most two edges from the same neighbour can await processing
  // at any time, hence the need for this buffer)
  Msg msgInBuffer[4];
  uint8_t msgInBufferValid[4];
  for (int i = 0; i < 4; i++) msgInBufferValid[i] = 0;

  // Receive buffer
  for (int i = 0; i < 3; i++) tinselAlloc(tinselSlot(4+i));

  // Zero initial subgrid
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 8; j++)
      subgrid[i][j] = 0;

  // Zero output messages
  for (int d = 0; d < 4; d++)
    for (int i = 0; i < 8; i++)
      msgOut[d]->temp[i] = 0;

  // Apply heat at north edge
  if (neighbour[N] < 0)
    for (int i = 0; i < 8; i++)
      msgIn[N].temp[i] = FixedPoint(255, 0);

  // Apply heat at west edge
  if (neighbour[W] < 0)
    for (int i = 0; i < 8; i++)
      msgIn[W].temp[i] = FixedPoint(255, 0);

  // Apply cool at south edge
  if (neighbour[S] < 0)
    for (int i = 0; i < 8; i++)
      msgIn[S].temp[i] = FixedPoint(40, 0);

  // Apply cool at east edge
  if (neighbour[E] < 0)
    for (int i = 0; i < 8; i++)
      msgIn[E].temp[i] = FixedPoint(40, 0);

  // Messages will be comprised of 3 flits
  tinselSetLen(2);

  // Simulation
  // ----------

  // Track number of messages received and sent for current time step
  int received = 0;
  int sent = 0;

  // Number of time steps to simulate
  const int nsteps = 50000;

  // Simulation loop
  for (int t = 0; t < nsteps; t++) {
   // Send/receive loop
    while (received < numNeighbours || sent < numNeighbours) {
      // Compute wait condition
      TinselWakeupCond cond =
         TINSEL_CAN_RECV | (sent < numNeighbours ? TINSEL_CAN_SEND : 0);

      // Wait to send or receive message
      tinselWaitUntil(cond);

      // Send edge temperatures
      if (sent < numNeighbours && tinselCanSend()) {
        int dir = neighbourList[sent];
        // Send edge to neighbour
        msgOut[dir]->time = t;
        msgOut[dir]->from = opposite(dir);
        tinselSend(neighbour[dir], msgOut[dir]);
        // Move on to next neighbour
        sent++;
      }

      // Receive edge temperatures
      if (tinselCanRecv()) {
        volatile Msg* msg = tinselRecv();
        if (msg->time == t) {
          // For current time step
          msgIn[msg->from] = *msg;
          received++;
        }
        else {
          // For next time step
          msgInBuffer[msg->from] = *msg;
          msgInBufferValid[msg->from] = 1;
        }
        tinselAlloc(msg);
      }
    }

    // Compute new temperatures
    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 8; x++) {
        // Determine neighbourhood samples
        int n = y == 0 ? msgIn[N].temp[x] : subgrid[y-1][x];
        int s = y == 7 ? msgIn[S].temp[x] : subgrid[y+1][x];
        int e = x == 7 ? msgIn[E].temp[y] : subgrid[y][x+1];
        int w = x == 0 ? msgIn[W].temp[y] : subgrid[y][x-1];
        // New temperature
        newSubgrid[y][x] = subgrid[y][x] - (subgrid[y][x] - ((n+s+e+w) >> 2));
        // Update output edges (to be sent to neighbours)
        if (y == 0) msgOut[N]->temp[x] = newSubgrid[y][x];
        if (y == 7) msgOut[S]->temp[x] = newSubgrid[y][x];
        if (x == 7) msgOut[E]->temp[y] = newSubgrid[y][x];
        if (x == 0) msgOut[W]->temp[y] = newSubgrid[y][x];
      }
    }

    // Recognise any buffered edges
    received = 0;
    for (int i = 0; i < 4; i++)
      if (msgInBufferValid[i]) {
        msgIn[i] = msgInBuffer[i];
        msgInBufferValid[i] = 0;
        received++;
      }

    // Prepare for next iteration
    subgridPtr = subgrid; subgrid = newSubgrid; newSubgrid = subgridPtr;
    sent = 0;
  }

  // Emit grid
  emitGrid(subgrid);

  return 0;
}
