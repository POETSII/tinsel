// Simulate heat diffusion on a LENxLEN grid of threads.
// Each thread handles an 8x8 subgrid of cells.
#define LEN 32

#include <tinsel.h>

// 32-bit fixed-point number in 16.16 format
#define FixedPoint(x, y) (((x) << 16) | (y))

// Poles
// -----

// Direction: north, south, east, and west
typedef enum {N=0, S=1, E=2, W=3} Dir;

// Given a direction, return opposite direction
inline Dir opposite(Dir d)
{
  if (d == N) return S;
  if (d == S) return N;
  if (d == E) return W;
  return E;
}

// Mapping subgrids to threads and back
// ------------------------------------

// Thread with id T gets mapped to subgrid with coords X and Y where X
// is the even bits of T and Y is the odd bits of T.  This keeps
// neighbouring subgrids near to each other in the hardware.

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
void myXY(int* x, int* y)
{
  int me = tinselId();
  *x = evens(me);
  *y = odds(me);
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

// Determine thread id from X and Y coords of subgrid
int fromXY(int x, int y)
{
  return unevens(x) | unodds(y);
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

// Top-level
// =========

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

  int xPos, yPos;
  myXY(&xPos, &yPos);

  // Neighbours
  // ----------

  // The thread to the north, south, east, and west
  int neighbour[4];
  neighbour[N] = yPos == 0     ? -1 : fromXY(xPos, yPos-1);
  neighbour[S] = yPos == LEN-1 ? -1 : fromXY(xPos, yPos+1);
  neighbour[E] = xPos == LEN-1 ? -1 : fromXY(xPos+1, yPos);
  neighbour[W] = xPos == 0     ? -1 : fromXY(xPos-1, yPos);

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
  volatile Msg* msgIn[4];
  for (int i = 0; i < 4; i++) {
    msgIn[i] = tinselSlot(i+4);
    if (neighbour[i] >= 0) tinselAlloc(msgIn[i]);
  }

  // Buffer for messages received from neighbours
  // (At most two edges from the same neighbour can await processing
  // at any time, hence the need for this buffer)
  volatile Msg* msgInBuffer[4];
  for (int i = 0; i < 4; i++) {
    msgInBuffer[i] = 0;
    tinselAlloc(tinselSlot(i+8));
  }

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
      msgIn[N]->temp[i] = FixedPoint(255, 0);

  // Apply heat at west edge
  if (neighbour[W] < 0)
    for (int i = 0; i < 8; i++)
      msgIn[W]->temp[i] = FixedPoint(255, 0);

  // Apply cool at south edge
  if (neighbour[S] < 0)
    for (int i = 0; i < 8; i++)
      msgIn[S]->temp[i] = FixedPoint(40, 0);

  // Apply cool at east edge
  if (neighbour[E] < 0)
    for (int i = 0; i < 8; i++)
      msgIn[E]->temp[i] = FixedPoint(40, 0);

  // Messages will be comprised of 3 flits
  tinselSetLen(2);

  // Simulation
  // ----------

  // Track number of messages received and sent for current time step
  int received = 0;
  int sent = 0;

  // Number of time steps to simulate
  const int nsteps = 100000;

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
          msgIn[msg->from] = msg;
          received++;
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
        int n = y == 0 ? msgIn[N]->temp[x] : subgrid[y-1][x];
        int s = y == 7 ? msgIn[S]->temp[x] : subgrid[y+1][x];
        int e = x == 7 ? msgIn[E]->temp[y] : subgrid[y][x+1];
        int w = x == 0 ? msgIn[W]->temp[y] : subgrid[y][x-1];
        // New temperature
        newSubgrid[y][x] = subgrid[y][x] - (subgrid[y][x] - ((n+s+e+w) >> 2));
        // Update output edges (to be sent to neighbours)
        if (y == 0) msgOut[N]->temp[x] = newSubgrid[y][x];
        if (y == 7) msgOut[S]->temp[x] = newSubgrid[y][x];
        if (x == 7) msgOut[E]->temp[y] = newSubgrid[y][x];
        if (x == 0) msgOut[W]->temp[y] = newSubgrid[y][x];
      }
    }

    // Reallocate space for used messages
    for (int i = 0; i < numNeighbours; i++) {
      Dir d = neighbourList[i];
      tinselAlloc(msgIn[d]);
    }

    // Recognise any buffered edges
    received = 0;
    for (int i = 0; i < 4; i++)
      if (msgInBuffer[i] != 0) {
        msgIn[i] = msgInBuffer[i];
        msgInBuffer[i] = 0;
        received++;
      }

    // Prepare for next iteration
    subgridPtr = subgrid; subgrid = newSubgrid; newSubgrid = subgridPtr;
    sent = 0;
  }

  // Finally, emit the state of the local subgrid
  int x = xPos * 8;
  int y = yPos * 8;
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 8; j++) {
      // Transfer Y coord (12 bits), X coord (12 buts) and temp (8 bits)
      uint32_t coords = ((y+i) << 12) | (x+j);
      uint32_t temp = (subgrid[i][j] >> 16) & 0xff;
      tinselHostPut((coords << 8) | temp);
    }

  return 0;
}
