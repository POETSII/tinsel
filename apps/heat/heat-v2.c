// Simulate heat diffusion on a NxN grid of threads.
// Each thread handles an LxL subgrid of samples,
// where L is a power of 2 and a mulitple of 8.
#define LOG_L 4
#define L (1 << LOG_L)

#include <tinsel.h>

// 32-bit fixed-point number in 16.16 format
#define FixedPoint(x, y) (((x) << 16) | (y))

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

// Determine thread id from X and Y coords
inline int fromXY(int x, int y)
{
  const uint32_t logLen = TinselLogThreadsPerBoard >> 1;
  return (y << logLen) | x;
}

// Message format
typedef struct {
  // The time step that the sender is on
  int time;
  // Which direction did the message come from
  Dir from;
  // Edge-offset of temperatures
  int offset;
  // Temperatures from the sender
  int temp[8];
} Msg;

int main()
{
  // Thread id
  int me = tinselId();

  // Subgrid location
  // ----------------

  // Length of square grid of threads
  const uint32_t logLen = TinselLogThreadsPerBoard >> 1;
  const uint32_t len    = 1 << logLen;

  // X and Y position of thread in grid
  const uint32_t xPos = me & (len-1);
  const uint32_t yPos = me >> logLen;

  // Neighbours
  // ----------

  // The thread to the north, south, east, and west
  int neighbour[4];
  neighbour[N] = yPos == 0     ? -1 : fromXY(xPos, yPos-1);
  neighbour[S] = yPos == len-1 ? -1 : fromXY(xPos, yPos+1);
  neighbour[E] = xPos == len-1 ? -1 : fromXY(xPos+1, yPos);
  neighbour[W] = xPos == 0     ? -1 : fromXY(xPos-1, yPos);

  // List containing each neighbour
  // (Only the first numNeighbours elements are valid)
  Dir neighbourList[4];
  int numNeighbours = 0;
  for (int i = 0; i < 4; i++)
    if (neighbour[i] >= 0) neighbourList[numNeighbours++] = i;

  // Subgrids
  // --------

  // Allocate two LxL subgrids: one holds temperatures for the current
  // time step; the other holds temperatures for the next time step.
  int subgridSpace[L][L];
  int subgridSpaceNext[L][L];

  // Pointers to subgrids
  int (*subgrid)[L] = subgridSpace;
  int (*subgridNext)[L] = subgridSpaceNext;
  int (*subgridPtr)[L];

  // Allocate space for subgrid edges
  int edgeSpace[4][L];
  int edgeSpaceNext[4][L];

  // Pointers to edges
  int (*edge)[L] = edgeSpace;
  int (*edgeNext)[L] = edgeSpaceNext;
  int (*edgePtr)[L];

  // Initial state
  // -------------

  // Initialise subgrid
  for (int y = 0; y < L; y++)
    for (int x = 0; x < L; x++)
      subgrid[y][x] = 0;

  // Border temperatures
  for (int i = 0; i < L; i++) {
    // SE border is cool
    edge[S][i] = edgeNext[S][i] = neighbour[S] < 0 ? FixedPoint(40, 0) : 0;
    edge[E][i] = edgeNext[E][i] = neighbour[E] < 0 ? FixedPoint(40, 0) : 0;
    // NW border is hot
    edge[N][i] = edgeNext[N][i] = neighbour[N] < 0 ? FixedPoint(255, 0) : 0;
    edge[W][i] = edgeNext[W][i] = neighbour[W] < 0 ? FixedPoint(255, 0) : 0;
  }

  // Message to be sent to neighbours
  volatile Msg* msgOut = tinselSlot(0);

  // Allocate receive buffer
  const int ReceiveBufferSize = 4;
  for (int i = 0; i < ReceiveBufferSize; i++) tinselAlloc(tinselSlot(1+i));

  // Track number of messages received and sent for current time step
  int received = 0;
  int sent = 0;

  // Track number of messages received for next time step
  int receivedNext = 0;

  // Count number of edge fragments that have been sent
  int offset = 0;

  // Current neighbour that we are sending to
  int currentNeighbour = 0;

  // Number of messages sent/received by thread on each iteration
  const int numMsgs = numNeighbours*(L/8);

  // Use message comprising three flits
  tinselSetLen(2);

  // Simulation
  // ----------

  // Number of time steps to simulate
  const int nsteps = 1000;

  // Simulation loop
  for (int t = 0; t < nsteps; t++) {
    // Send/receive loop
    while (received < numMsgs || sent < numMsgs) {
      // Compute wait condition
      TinselWakeupCond cond =
         TINSEL_CAN_RECV | (sent < numMsgs ? TINSEL_CAN_SEND : 0);

      // Wait to send or receive message
      tinselWaitUntil(cond);

      // Send edge temperatures
      if (sent < numMsgs && tinselCanSend()) {
        int dir = neighbourList[currentNeighbour];
        // Send partial edge to neighbour
        msgOut->time = t;
        msgOut->from = opposite(dir);
        msgOut->offset = offset;
        // Temperatures to send
        int base = offset*8;
        if (dir == N) for (int i = 0; i < 8; i++)
          msgOut->temp[i] = subgrid[0][base+i];
        else if (dir == S) for (int i = 0; i < 8; i++)
          msgOut->temp[i] = subgrid[L-1][base+i];
        else if (dir == E) for (int i = 0; i < 8; i++)
          msgOut->temp[i] = subgrid[base+i][L-1];
        else for (int i = 0; i < 8; i++)
          msgOut->temp[i] = subgrid[base+i][0];
        // Send message
        tinselSend(neighbour[dir], msgOut);
        offset++; sent++;
        // When full edge is sent, move on to next neighbour
        if (offset == (L/8)) {
          offset = 0;
          currentNeighbour++;
        }
      }

      // Receive edge temperatures
      if (tinselCanRecv()) {
        volatile Msg* msg = tinselRecv();
        int* ptr;
        if (msg->time == t) {
          // Temperature for current time step
          ptr = &edge[msg->from][8*msg->offset];
          received++;
        }
        else {
          // Temperature for next time step
          ptr = &edgeNext[msg->from][8*msg->offset];
          receivedNext++;
        }
        for (int i = 0; i < 8; i++) ptr[i] = msg->temp[i];
        // Reallocate message slot for new incoming messages
        tinselAlloc(msg);
      }
    }

    // Compute new temperatures
    for (int y = 0; y < L; y++) {
      for (int x = 0; x < L; x++) {
        // Determine neighbourhood samples
        int n = y == 0     ? edge[N][x] : subgrid[y-1][x];
        int s = y == (L-1) ? edge[S][x] : subgrid[y+1][x];
        int e = x == (L-1) ? edge[E][y] : subgrid[y][x+1];
        int w = x == 0     ? edge[W][y] : subgrid[y][x-1];
        // New temperature
        subgridNext[y][x] = subgrid[y][x] - (subgrid[y][x] - ((n+s+e+w) >> 2));
      }
    }

    // Prepare for next iteration
    subgridPtr = subgrid; subgrid = subgridNext; subgridNext = subgridPtr;
    edgePtr = edge; edge = edgeNext; edgeNext = edgePtr;
    received = receivedNext;
    currentNeighbour = receivedNext = sent = offset = 0;
  }

  // Emit local subgrid
  int x = xPos * L;
  int y = yPos * L;
  for (int i = 0; i < L; i++)
    for (int j = 0; j < L; j++) {
      // Transfer Y coord (12 bits), X coord (12 bits) and temp (8 bits)
      uint32_t coords = ((y+i) << 12) | (x+j);
      uint32_t temp = (subgrid[i][j] >> 16) & 0xff;
      tinselHostPut((coords << 8) | temp);
    }

  return 0;
}
