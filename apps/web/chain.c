// Create a chain comprising the grid of threads in raster scan order.
// Each thread in the chain receives a colour from the previous
// thread, inverts it, and sends it to the next to produce a series of
// stripes.  The first thread in the chain, which doesn't have a
// previous thread, triggers the flow with an initial message.

#include <tinsel.h>
#include <grid.h>

int main()
{
  // Id for this thread
  int me = tinselId();

  // Get pointer to a mailbox message slot
  volatile int* out = tinselSlot(0);

  if (me == 0) {
    // Emit colour 0
    emit(0);
    // Create message containing colour 255
    out[0] = 255;
    // Wait until message-send is possible
    tinselWaitUntil(TINSEL_CAN_SEND);
    // Send message to next thread
    tinselSend(fromXY(1, 0), out);
  }
  else {
    // Get pointer to another mailbox message slot
    volatile int* in = tinselSlot(0);
    // Allocate space to receive message
    tinselAlloc(in);
    // Wait until message is available
    tinselWaitUntil(TINSEL_CAN_RECV);
    // Receive colour
    tinselRecv();
    // Emit colour
    emit(in[0]);
    // Create message containing inverted colour
    out[0] = in[0] == 0 ? 255 : 0;
    // Wait until message-send is possible
    tinselWaitUntil(TINSEL_CAN_SEND);
    // Send message to next thread
    int nextX = east() < 0 ? 0 : getX(me)+1;
    int nextY = east() < 0 ? getY(me)+1 : getY(me);
    tinselSend(fromXY(nextX, nextY), out);
  }

  return 0;
}
