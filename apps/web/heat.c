// Simulate heat diffusion with a grid of threads.

#include <tinsel.h>
#include <grid.h>

// 32-bit fixed-point number in 16.16 format
#define FixedPoint(x, y) (((x) << 16) | (y))

// Message format
typedef struct {
  // The time step that the sender is on
  int time;
  // The temperature of the sender
  int temp;
} Msg;

int main()
{
  // Neighbours
  // ----------

  // The thread to the north, south, east, and west
  int neighbour[4];
  neighbour[N] = north();
  neighbour[S] = south();
  neighbour[E] = east();
  neighbour[W] = west();

  // List containing each neighbour
  // (Only the first numNeighbours elements are valid)
  int neighbourList[4];
  int numNeighbours = 0;
  for (int i = 0; i < 4; i++)
    if (neighbour[i] >= 0) neighbourList[numNeighbours++] = neighbour[i];

  // Initial state
  // -------------

  // Border temperature (only non-zero for threads on border)
  int borderTemp = 0;

  // NW border is hot
  if (neighbour[N] < 0) borderTemp += FixedPoint(255, 0);
  if (neighbour[W] < 0) borderTemp += FixedPoint(255, 0);

  // SE border is cool
  if (neighbour[S] < 0) borderTemp += FixedPoint(40, 0);
  if (neighbour[E] < 0) borderTemp += FixedPoint(40, 0);

  // Temperature
  int temp = 0;
  int acc = 0;
  int accNext = 0;

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

  // Simulation
  // ----------

  // Number of time steps to simulate
  const int nsteps = 5000;

  // Simulation loop
  for (int t = 0; t < nsteps; t++) {
    // Send/receive loop
    while (received < numNeighbours || sent < numNeighbours) {
      // Compute wait condition
      TinselWakeupCond cond =
         TINSEL_CAN_RECV | (sent < numNeighbours ? TINSEL_CAN_SEND : 0);

      // Wait to send or receive message
      tinselWaitUntil(cond);

      // Send temperature
      if (sent < numNeighbours && tinselCanSend()) {
        int neighbour = neighbourList[sent++];
        msgOut->time = t;
        msgOut->temp = temp;
        tinselSend(neighbour, msgOut);
      }

      // Receive neighbouring temperature
      if (tinselCanRecv()) {
        volatile Msg* msg = tinselRecv();
        if (msg->time == t) {
          // Temperature for current time step
          acc += msg->temp;
          received++;
        }
        else {
          // Temperature for next time step
          accNext += msg->temp;
          receivedNext++;
        }
        // Reallocate message slot for new incoming messages
        tinselAlloc(msg);
      }
    }

    // Average of surrounding temperatures
    acc = (borderTemp + acc) >> 2;

    // Update temperature
    temp = temp - (temp - acc);

    // Prepare for next iteration
    acc = accNext;
    accNext = 0;
    received = receivedNext;
    receivedNext = 0;
    sent = 0;
  }

  // Emit the final temperature
  emit(temp >> 16);

  return 0;
}
