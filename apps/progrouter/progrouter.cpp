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

  // Number of records used so far in current beat
  uint32_t numRecords;

  // Index of beat currently being filled
  uint32_t currentBeat;

  // Constructor
  RoutingTable() {
    currentBeat = 0;
    numChunks = numRecords = 0;
  }

  // Pointer to current beat being filled
  uint8_t* currentPointer() {
    return beats[currentBeat].bytes;
  }

  // Move on to next the beat
  void next() {
    beats[currentBeat].bytes[31] = 0;
    beats[currentBeat].bytes[30] = numRecords;
    numChunks = 0;
    numRecords = 0;
    currentBeat++;
  }

  // Add a URM1 record to the table
  void addURM1(uint32_t mboxX, uint32_t mboxY,
                 uint32_t mboxThread, uint32_t localKey) {
    if (numChunks == 6) next();
    uint8_t* ptr = beats[currentBeat].bytes + 5*(5-numChunks);
    ptr[0] = localKey;
    ptr[1] = localKey >> 8;
    ptr[2] = localKey >> 16;
    ptr[3] = ((mboxThread&0x1f) << 3) | ((localKey >> 24) & 0x7);
    ptr[4] = (mboxY << 3) | (mboxX << 1) | (mboxThread >> 5);
    numChunks++;
    numRecords++;
  }

  // Add a URM2 record to the table
  void addURM2(uint32_t mboxX, uint32_t mboxY, uint32_t mboxThread,
                 uint32_t localKeyHigh, uint32_t localKeyLow) {
    if (numChunks >= 5) next();
    uint8_t* ptr = beats[currentBeat].bytes + 5*(4-numChunks);
    ptr[0] = localKeyLow;
    ptr[1] = localKeyLow >> 8;
    ptr[2] = localKeyLow >> 16;
    ptr[3] = localKeyLow >> 24;
    ptr[4] = localKeyHigh;
    ptr[5] = localKeyHigh >> 8;
    ptr[6] = localKeyHigh >> 16;
    ptr[7] = localKeyHigh >> 24;
    ptr[8] = (mboxThread&0x1f) << 3;
    ptr[9] = (1 << 5) | (mboxY << 3) | (mboxX << 1) | (mboxThread >> 5);
    numChunks += 2;
    numRecords++;
  }

  // Add an MRM record to the table
  void addMRM(uint32_t mboxX, uint32_t mboxY,
                uint32_t threadsHigh, uint32_t threadsLow) {
    if (numChunks >= 5) next();
    uint8_t* ptr = beats[currentBeat].bytes + 5*(4-numChunks);
    ptr[0] = threadsLow;
    ptr[1] = threadsLow >> 8;
    ptr[2] = threadsLow >> 16;
    ptr[3] = threadsLow >> 24;
    ptr[4] = threadsHigh;
    ptr[5] = threadsHigh >> 8;
    ptr[6] = threadsHigh >> 16;
    ptr[7] = threadsHigh >> 24;
    ptr[9] = (3 << 5) | (mboxY << 3) | (mboxX << 1);
    numChunks += 2;
    numRecords++;
  }

  // Add an IND record to the table
  // Return a pointer to the indirection key,
  // so it can be set later by the caller
  uint8_t* addIND() {
    if (numChunks == 6) next();
    uint8_t* ptr = beats[currentBeat].bytes + 5*(5-numChunks);
    ptr[4] = 4 << 5;
    numChunks++;
    numRecords++;
    return ptr;
  }

  // Set indirection key
  void setIND(uint8_t* ind, bool upperRam,
                uint8_t* beatPtr, uint32_t numBeats) {
    uint32_t key = (uint32_t) beatPtr | numBeats;
    if (upperRam) key |= 0x80000000;
    ind[0] = key;
    ind[1] = key >> 8;
    ind[2] = key >> 16;
    ind[3] = key >> 24;
  }

  // Add an RR record to the table
  void addRR(uint32_t dir, uint32_t key) {
    if (numChunks == 6) next();
    uint8_t* ptr = beats[currentBeat].bytes + 5*(5-numChunks);
    ptr[0] = key;
    ptr[1] = key >> 8;
    ptr[2] = key >> 16;
    ptr[3] = key >> 24;
    ptr[4] = (2 << 5) | (dir << 3);
    numChunks++;
    numRecords++;
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
    uint8_t* entry1 = table.currentPointer();
    table.addURM1(0, 0, 10, 0xfff);
    table.addURM2(0, 0, 60, 0xff1, 0xff0);
    table.addURM2(0, 0, 60, 0xff3, 0xff2);
    table.addURM2(0, 0, 60, 0xff5, 0xff4);
    //table.addMRM(1, 0, 0x22222222, 0x11111111);
    uint8_t* ind = table.addIND();
    table.next();
    uint8_t* entry2 = table.currentPointer();
    table.addURM1(0, 0, 20, 0x111);
    table.next();
    table.setIND(ind, 0, entry2, 1);

    // Cache flush, to write table into RAM
    tinselCacheFlush();
    // Wait until flush done, by issuing a load
    volatile uint32_t* dummyPtr = (uint32_t*) entry1; dummyPtr[0];

    // Construct key
    uint32_t key = (uint32_t) entry1;
    key = key | 2; // Entry is 2 beats long

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
