// SPDX-License-Identifier: BSD-2-Clause
// Simulate heat diffusion on an X_LEN x Y_LEN grid of threads.
// Each thread handles an 8x8 subgrid of cells.

#include <tinsel.h>
#include <assert.h>
#include "heat.h"

// 32-bit fixed-point number in 16.16 format
#define FixedPoint(x, y) (((x) << 16) | (y))

// Poles
// -----

// Direction: north, south, east, and west
typedef enum {N=0, S=1, E=2, W=3} Dir;

// Given a direction, return opposite direction
inline Dir opposite(Dir d) { return d^1; }

// Mapping subgrids to threads and back
// ------------------------------------

// A thread with board-local thread id T gets mapped to a subgrid of
// the heat surface with coords X and Y where X is the even bits of T
// and Y is the odd bits of T.  These board-local X and Y coords are
// then combined with the board's X and Y mesh coords to give the
// final subgrid coords.  This scheme keeps neighbouring subgrids near
// to each other in the hardware.

// Obtain the board-local id and the mesh X and Y coords of the board
void fromId(uint32_t* local, uint32_t* meshX, uint32_t* meshY)
{
  uint32_t me = tinselId();
  *local = me % (1 << TinselLogThreadsPerBoard);
  *meshX = (me >> TinselLogThreadsPerBoard) % (1 << TinselMeshXBits);
  *meshY = me >> (TinselLogThreadsPerBoard + TinselMeshXBits);
} 

// Inverse of the above function
uint32_t toId(uint32_t local, uint32_t meshX, uint32_t meshY)
{
  return local
       | (meshX << TinselLogThreadsPerBoard)
       | (meshY << (TinselLogThreadsPerBoard + TinselMeshXBits));
}

// Return bits at even-numbered indexes
uint32_t evens(uint32_t in)
{
  uint32_t out = 0;
  for (int i = 0; i < 16; i++) {
    out = out | ((in&1) << i);
    in >>= 2;
  }
  return out;
}

// Return bits at odd-numbered indexes
uint32_t odds(uint32_t in)
{
  return evens(in >> 1);
}

// Determine X and Y coords of thread's subgrid
void myXY(uint32_t* x, uint32_t* y)
{
  uint32_t local, meshX, meshY;
  fromId(&local, &meshX, &meshY);
  *x = evens(local) + (meshX * X_LOCAL_LEN);
  *y = odds(local) + (meshY * Y_LOCAL_LEN);
}

// Partial inverse of evens
uint32_t unevens(uint32_t in)
{
  uint32_t out = 0;
  for (int i = 0; i < 16; i++) {
    out = out | ((in&1) << (2*i));
    in >>= 1;
  }
  return out;
}

// Partial inverse of odds
uint32_t unodds(uint32_t in)
{
  return unevens(in) << 1;
}

// Determine thread id from subgrid X and Y coords
uint32_t fromXY(uint32_t x, uint32_t y)
{
  return toId(unevens(x%X_LOCAL_LEN) |
              unodds(y%Y_LOCAL_LEN), x/X_LOCAL_LEN, y/Y_LOCAL_LEN);
}

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

static_assert(sizeof(Msg) < TinselBytesPerMsg,
  "Msg structure too large");
static_assert(sizeof(HostMsg) < TinselBytesPerMsg,
  "HostMsg structure too large");

// Output
// ------

// Emit the state of the thread-local subgrid
void emitGrid(int (*subgrid)[8])
{
  // Determine X and Y position of thread
  uint32_t xPos, yPos;
  myXY(&xPos, &yPos);
  // Determine X and Y pixel coordinates
  uint32_t x = xPos * 8;
  uint32_t y = yPos * 8;
  // Host id
  uint32_t hostId = tinselHostId();
  // Message to be sent to the host
  volatile HostMsg* msg = tinselSendSlot();
  // Send subgrid to host
  for (int i = 0; i < 8; i++) {
    tinselWaitUntil(TINSEL_CAN_SEND);
    msg->x = x;
    msg->y = y+i;
    for (int j = 0; j < 8; j++)
      msg->temps[j] = (subgrid[i][j] >> 16);
    tinselSend(hostId, msg);
  }
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
  myXY(&xPos, &yPos);

  // Suspend threads that are not being used
  if (xPos >= X_LEN || yPos >= Y_LEN) tinselWaitUntil(TINSEL_CAN_RECV);

  // Neighbours
  // ----------

  // The thread to the north, south, east, and west
  int neighbour[4];
  neighbour[N] = yPos == 0       ? -1 : fromXY(xPos, yPos-1);
  neighbour[S] = yPos == Y_LEN-1 ? -1 : fromXY(xPos, yPos+1);
  neighbour[E] = xPos == X_LEN-1 ? -1 : fromXY(xPos+1, yPos);
  neighbour[W] = xPos == 0       ? -1 : fromXY(xPos-1, yPos);

  // List containing each neighbour
  // (Only the first numNeighbours elements are valid)
  Dir neighbourList[4];
  int numNeighbours = 0;
  for (int i = 0; i < 4; i++)
    if (neighbour[i] >= 0) neighbourList[numNeighbours++] = i;

  // Initial state
  // -------------

  // Messages to be sent to neighbours
  Msg msgOut[4];

  // Messages received from neighbours
  volatile Msg msgIn[4];

  // Buffer for messages received from neighbours
  // (At most two edges from the same neighbour can await processing
  // at any time, hence the need for this buffer)
  volatile Msg* msgInBuffer[4];
  for (int i = 0; i < 4; i++) msgInBuffer[i] = 0;

  // Zero initial subgrid
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 8; j++)
      subgrid[i][j] = 0;

  // Zero output messages
  for (int d = 0; d < 4; d++)
    for (int i = 0; i < 8; i++)
      msgOut[d].temp[i] = 0;

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
        volatile Msg* msg = tinselSendSlot();
        msgOut[dir].time = t;
        msgOut[dir].from = opposite(dir);
        *msg = msgOut[dir];
        tinselSend(neighbour[dir], msg);
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
          tinselFree(msg);
        }
        else {
          // For next time step
          msgInBuffer[msg->from] = msg;
        }
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
        if (y == 0) msgOut[N].temp[x] = newSubgrid[y][x];
        if (y == 7) msgOut[S].temp[x] = newSubgrid[y][x];
        if (x == 7) msgOut[E].temp[y] = newSubgrid[y][x];
        if (x == 0) msgOut[W].temp[y] = newSubgrid[y][x];
      }
    }

    // Recognise any buffered edges
    received = 0;
    for (int i = 0; i < 4; i++)
      if (msgInBuffer[i] != 0) {
        msgIn[i] = *msgInBuffer[i];
        tinselFree(msgInBuffer[i]);
        msgInBuffer[i] = 0;
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
