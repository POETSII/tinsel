// Simulate heat diffusion on a 32x32 grid of threads.

#include <tinsel.h>
#include <grid.h>

// 32-bit fixed-point number in 16.16 format
#define FixedPoint(x, y) (((x) << 16) | (y))

// Message format
typedef struct {
  // Which direction (N, S, E, or W) did the message come from?
  Dir from;
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
  Dir neighbourList[4];
  int numNeighbours = 0;
  for (int i = 0; i < 4; i++)
    if (neighbour[i] >= 0) neighbourList[numNeighbours++] = i;

  // Initial state
  // -------------

  // Initial temperature
  int temp = 0;

  // Message to be sent to neighbours
  volatile Msg* msgOut = tinselSlot(0);

  // Messages received from neighbours
  volatile Msg* msgIn[4];
  for (int i = 0; i < 4; i++) {
    msgIn[i] = tinselSlot(i+1);
    msgIn[i]->temp = 0;
  }

  // Initally NW border is hot
  if (neighbour[N] < 0 || neighbour[W] < 0) {
    msgIn[N]->temp = FixedPoint(255, 0);
    msgIn[W]->temp = FixedPoint(255, 0);
  }

  // Initially SE border is cool
  if ( neighbour[S] < 0 || neighbour[E] < 0) {
    msgIn[S]->temp = FixedPoint(40, 0);
    msgIn[E]->temp = FixedPoint(40, 0);
  }

  // Buffer for temperatures received from neighbours
  volatile Msg* msgBuffer[4];
  for (int i = 0; i < 4; i++) {
    msgBuffer[i] = 0;
    tinselAlloc(tinselSlot(i+5));
  }

  // Simulation
  // ----------

  // Number of time steps to simulate
  const int nsteps = 1000;

  // Simulation loop
  for (int t = 0; t < nsteps; t++) {

    // Ensure all outstanding messages have been sent
    tinselWaitUntil(TINSEL_CAN_SEND);

    // Average of surrounding temperatures
    int surroundings = ( msgIn[N]->temp + msgIn[S]->temp +
                         msgIn[E]->temp + msgIn[W]->temp ) >> 2;

    // Compute new temperature for this cell
    int newTemp = temp - (temp - surroundings);

    // Allocate space to receive messages
    for (int i = 0; i < numNeighbours; i++) {
      Dir d = neighbourList[i];
      tinselAlloc(msgIn[d]);
    }

    // Counts of messages sent and received
    int numSent = 0;
    int numReceived = 0;

    // Recognise any buffered temperatures
    for (int i = 0; i < 4; i++)
      if (msgBuffer[i] != 0) {
        msgIn[i] = msgBuffer[i];
        msgBuffer[i] = 0;
        numReceived++;
      }

    // Send & receive new temperatures
    for (;;) {
      uint32_t needToSend = numSent < numNeighbours;
      uint32_t needToRecv = numReceived < numNeighbours;
      if (!needToSend && !needToRecv) break;
      uint32_t waitCond = (needToSend ? TINSEL_CAN_SEND : 0)
                        | (needToRecv ? TINSEL_CAN_RECV : 0);

      // Suspend thread
      tinselWaitUntil(waitCond);

      // Send handler
      if (needToSend && tinselCanSend()) {
        // Get direction of next neighbour
        Dir d = neighbourList[numSent++];
        // Send message to neigbour
        msgOut->from = opposite(d);
        msgOut->time = t;
        msgOut->temp = newTemp;
        tinselSend(neighbour[d], msgOut);
      }

      // Receive handler
      if (tinselCanRecv()) {
        // Receive message from neighbour
        volatile Msg* msg = tinselRecv();
        // Is the received message for the current time step?
        if (msg->time == t) {
          msgIn[msg->from] = msg;
          numReceived++; 
        }
        else msgBuffer[msg->from] = msg;
      }
    }

    // Update temperature
    temp = newTemp;
  }

  // Emit the final temperature
  emit(temp >> 16);

  return 0;
}
