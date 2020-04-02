#include <tinsel.h>

// Simplest possible example involving programmable routers

/*
Byte ordering in a routing beat:

  31: Upper byte of length (i.e. number of records in beat)
  30: Lower byte of length
  29: Upper byte of first chunk 
  28:
  27:
  26:
  25: Lower byte of first chunk 
  24: Upper byte of second chunk 
  23:
  22:
  21:
  20: Lower byte of second chunk
  19: Upper byte of third chunk
  18:
  17:
  16:
  15: Lower byte of third chunk
  14: Upper byte of fourth chunk
  13:
  12:
  11:
  10: Lower byte of fourth chunk
   9: Upper byte of fifth chunk
   8:
   7:
   6:
   5: Lower byte of fifth chunk
   4: Upper byte of sixth chunk
   3:
   2:
   1:
   0: Lower byte of sixth chunk

Need to fold this into the docs eventually.
*/

// Use this to align on beat boundary
#define ALIGNED __attribute__((aligned(32)))

// A single RAM beat
struct ALIGNED Beat {
  uint8_t bytes[32];
};

// Routing table, with methods to aid construction
template <int NumBeats> struct RoutingTable {
  // Raw beats comprising the table
  Beat beats[NumBeats];

  // Number of chunks used so far in current beat
  uint32_t numChunks;

  // Index of beat currently being filled
  uint32_t currentBeat;

  // Constructor
  RoutingTable() {
    currentBeat = 0;
  }

  // Pointer to current beat being filled
  uint8_t* currentPointer() {
    return beats[currentBeat].bytes;
  }

  // Move on to next the beat
  void next() {
    beats[currentBeat].bytes[31] = 0;
    beats[currentBeat].bytes[30] = numChunks;
    numChunks = 0;
    currentBeat++;
  }

  // Add a URM1 record to the table
  void addURM1(uint32_t mboxX, uint32_t mboxY,
                 uint32_t mboxThread, uint32_t localKey) {
    uint8_t* ptr = beats[currentBeat].bytes + 5*(5-numChunks);
    ptr[0] = localKey;
    ptr[1] = localKey >> 8;
    ptr[2] = localKey >> 16;
    ptr[3] = ((mboxThread&0x1f) << 3) | ((localKey >> 24) & 0x7);
    ptr[4] = (mboxY << 3) | (mboxX << 1) | (mboxThread >> 5);
    numChunks++;
    if (numChunks == 6) next();
  }
};

// Create global routing table of 16 beats
RoutingTable<16> table;

int main()
{
  // Get thread id
  int me = tinselId();

  // Sample outgoing message
  volatile uint32_t* msgOut = (uint32_t*) tinselSendSlot();
  msgOut[0] = 0x10;
  msgOut[1] = 0x20;
  msgOut[2] = 0x30;
  msgOut[3] = 0x40;

  // On thread 0
  if (me == 0) {
    // Add an URM1 record
    uint8_t* entry = table.currentPointer();
    table.addURM1(0, 0, 10, 0xff);
    table.next();

    // Cache flush, to write table into RAM
    tinselCacheFlush();
    // Wait until flush done, by issuing a load
    volatile uint32_t* dummyPtr = (uint32_t*) entry; dummyPtr[0];

    // Construct key
    uint32_t key = (uint32_t) entry;
    key = key | 1; // Entry is 1 beat long

    // Send message to key
    tinselWaitUntil(TINSEL_CAN_SEND);
    tinselKeySend(key, msgOut);

    while (1);
  }

  // On other threads, print anything received
  while (me != 0) {
    tinselWaitUntil(TINSEL_CAN_RECV);
    volatile uint32_t* msgIn = (uint32_t*) tinselRecv();
    printf("%x %x %x %x\n", msgIn[0], msgIn[1], msgIn[2], msgIn[3]);
    tinselFree(msgIn);
  }

  return 0;
}
