#include <tinsel.h>

// Simulate Newton's law of cooling on a 2D square grid of threads.
// Each thread simulates a subgrid of NxN cells.

// Direction: north, south, east, and west
typedef enum {N=0, S=1, E=2, W=3} Dir;

// Given a direction, return opposite direction
Dir opposite(Dir d)
{
  if (d == N) return S;
  if (d == S) return N;
  if (d == E) return W;
  return E;
}

int main()
{
  // Id for this thread
  const uint32_t me = myId();

  // Neighbours
  // ----------
  //
  // Logically, there is a square grid of threads.  (We assume
  // LogThreadsPerBoard is even, otherwise grid may not be square.)

  // Square length
  const uint32_t logLen = LogThreadsPerBoard >> 1;
  const uint32_t len    = 1 << logLen;

  // X and Y position of thread in grid
  const uint32_t xPos = me & (len-1);
  const uint32_t yPos = me >> logLen;

  // The thread to the north, south, east, and west
  // (-1 denotes no such thread, i.e. at the edges of the grid)
  int neighbour[4];
  neighbour[N] = yPos == 0     ? -1 : (((yPos-1) << logLen) | xPos);
  neighbour[S] = yPos == len-1 ? -1 : (((yPos+1) << logLen) | xPos);
  neighbour[E] = xPos == len-1 ? -1 : ((yPos << logLen) | (xPos+1));
  neighbour[W] = xPos == 0     ? -1 : ((yPos << logLen) | (xPos-1));

  // List containing each neighbour
  // (Only the first numNeighbours elements are valid)
  Dir neighbourList[4];
  int numNeighbours = 0;
  for (int i = 0; i < 4; i++)
    if (neighbour[i] >= 0) neighbourList[numNeighbours++] = i;

  // Subgrids
  // --------
  //
  // Each thread simulates a square subgrid of LxL cells.

  // We pick a subgrid length small enough for a whole subgrid edge to
  // be sent in a single message, with 1 word leftover to specify the
  // direction from which the message came.
  const uint32_t L = (1 << LogWordsPerMsg) - 1;

  // We allocate space for two subgrids (the current state and the
  // next state) on the stack.
  int subgridSpace[L][L];
  int newSubgridSpace[L][L];

  // Pointers to subgrids, used in a double-buffer fashion
  int (*subgrid)[L] = subgridSpace;
  int (*newSubgrid)[L] = newSubgridSpace;
  int (*gridPtr)[L];

  // Initial state
  // -------------

  // Edge temperatures to be sent to neighbours
  volatile int* edgeOut[4];
  for (int i = 0; i < 4; i++) edgeOut[i] = mboxSlot(i);

  // Edge temperatures received from neighbours
  volatile int* edgeIn[4];
  for (int i = 4; i < 8; i++) edgeIn[i-4] = mboxSlot(i);

  // Buffer for edge temperatures received from neighbours
  // (At most two edges from the same neighbour can await processing
  // at any time, hence the need for this buffer)
  volatile int* edgeInBuffer[4];
  for (int i = 0; i < 4; i++) {
    edgeInBuffer[i] = 0;
    mboxAlloc(mboxSlot(i+8));
  }

  // Zero initial subgrid
  for (int i = 0; i < L; i++)
    for (int j = 0; j < L; j++)
      subgrid[i][j] = 0;

  // Zero initial edges
  for (int i = 0; i < 4; i++)
    for (int j = 1; j <= L; j++)
      edgeIn[i][j] = 0;

  // Apply heat at north edge
  if (yPos == 0)
    for (int i = 1; i <= L; i++)
      edgeIn[N][i] = 255;

  // Apply heat at west edge
  if (xPos == 0)
    for (int i = 1; i <= L; i++)
      edgeIn[W][i] = 255;

  // Simulation
  // ----------

  // Number of time steps to simulate
  const int nsteps = 10;

  // Simulation loop
  for (int t = 0; t < nsteps; t++) {

    // Ensure no incomplete sends before continuing
    // (Don't want to modify edgeOut until all edges have been sent)
    mboxWaitUntil(CAN_SEND);

    // Update state
    for (int y = 0; y < L; y++) {
      for (int x = 0; x < L; x++) {
        // Values of left, right, above, and below cells
        int l = x == 0     ? edgeIn[W][y+1] : subgrid[y][x-1];
        int r = x == (L-1) ? edgeIn[E][y+1] : subgrid[y][x+1];
        int a = y == 0     ? edgeIn[N][x+1] : subgrid[y-1][x];
        int b = y == (L-1) ? edgeIn[S][x+1] : subgrid[y+1][x];
        // Average of surrounding temperatures
        int surroundings = (a + b + l + r) >> 2;
        // Compute new temperature for this cell
        // (Assuming dissapation constant of 0.25)
        int newTemp =
          subgrid[y][x] - ((subgrid[y][x] - surroundings) >> 2);
        // Update subgrid
        newSubgrid[y][x] = newTemp;
        // Update edges
        if (y == 0)   edgeOut[N][x+1] = newTemp;
        if (y == N-1) edgeOut[S][x+1] = newTemp;
        if (x == 0)   edgeOut[W][y+1] = newTemp;
        if (x == N-1) edgeOut[E][y+1] = newTemp;
      }
    }

    // Allocate space to receive edges
    for (int i = 0; i < numNeighbours; i++) {
      Dir d = neighbourList[i];
      mboxAlloc(edgeIn[d]);
    }

    // Counts of edges received and sent
    int edgesSent = 0;
    int edgesReceived = 0;

    // Recognise any buffered edges
    for (int i = 0; i < 4; i++)
      if (edgeInBuffer[i] != 0) {
        edgeIn[i] = edgeInBuffer[i];
        edgeInBuffer[i] = 0;
        edgesReceived++;
      }

    // Send & receive new edges
    for (;;) {
      uint32_t needToSend = edgesSent < numNeighbours;
      uint32_t needToRecv = edgesReceived < numNeighbours;
      if (!needToSend && !needToRecv) break;
      uint32_t waitCond = (needToSend ? CAN_SEND : 0)
                        | (needToRecv ? CAN_RECV : 0);

      // Suspend thread
      mboxWaitUntil(waitCond);

      // Send handler
      if (mboxCanSend()) {
        mboxSetLen(3);
        Dir d = neighbourList[edgesSent];
        // Send both the source direction and the LSB of the time step
        // in the first word of the message
        edgeOut[d][0] = (opposite(d) << 1) | (t & 1);
        mboxSend(neighbour[d], edgeOut[d]);
        edgesSent++;
      }

      // Receive handler
      if (mboxCanRecv()) {
        volatile int* msg = mboxRecv();
        Dir d = msg[0] >> 1;
        // Is the received edge for the current or next time step?
        if ((msg[0]&1) == (t&1)) {
          edgeIn[d] = msg;
          edgesReceived++; 
        } else {
          // Buffer the edge
          edgeInBuffer[d] = msg;
        }
      }
    }

    // Switch buffers for next time step
    gridPtr = subgrid;
    subgrid = newSubgrid;
    newSubgrid = gridPtr;
  }

  // Finally, emit the state of the local subgrid
  for (int y = 0; y < L; y++)
    for (int x = 0; x < L; x++) {
      hostPut(newSubgrid[y][x]);
    }

  return 0;
}
