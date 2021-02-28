// SPDX-License-Identifier: BSD-2-Clause
#ifndef _PROGROUTERS_H_
#define _PROGROUTERS_H_

#include <assert.h>
#include <config.h>
#include <HostLink.h>
#include <POLite.h>
#include <POLite/Seq.h>
#include <boot.h>

// =============================
// Per-board programmable router
// =============================

class ProgRouter {

  // Number of chunks used so far in current beat
  uint32_t numChunks;

  // Number of records used so far in current beat
  uint32_t numRecords;

  // Number of beats associated with current key
  uint32_t numBeats;

  // Index of RAM currently being used
  uint32_t currentRAM;

  // Pointer to previously created indirection
  // (We need indirections to handle record sequences of 31 beats or more)
  uint8_t* prevInd;

  // Move on to next the beat
  void nextBeat() {
    // Set number of records in current beat
    uint32_t beatBase = table[currentRAM]->numElems - 32;
    uint8_t* beat = &table[currentRAM]->elems[beatBase];
    beat[31] = 0;
    beat[30] = numRecords;
    numChunks = numRecords = 0;
    // Allocate new beat, and check for overflow
    numBeats++;
    table[currentRAM]->extendByAndZeroExtra(32);
    if (table[currentRAM]->numElems >= (TinselPOLiteProgRouterLength-1024)) {
      printf("ProgRouter out of memory\n");
      exit(EXIT_FAILURE);
    }
    // We need indirections to handle sequences of 31 beats or more
    if ((numBeats % 31) == 0) {
      // Set previous indirection, if there is one
      if (prevInd) {
        uint32_t key = TinselPOLiteProgRouterBase +
                         table[currentRAM]->numElems - 31*32;
        if (currentRAM) key |= 0x80000000;
        key |= 31;
        setIND(prevInd, key);
      }
      prevInd = addIND();
    }
  }

  // Get current record pointer for 48-bit entry
  inline uint8_t* currentRecord48() {
    uint32_t beatBase = (table[currentRAM]->numElems-32) + 6*(4-numChunks);
    return &table[currentRAM]->elems[beatBase];
  }

  // Get current record pointer for 96-bit entry
  inline uint8_t* currentRecord96() {
    uint32_t beatBase = (table[currentRAM]->numElems-32) + 6*(3-numChunks);
    return &table[currentRAM]->elems[beatBase];
  }

    // Set indirection key
  void setIND(uint8_t* ind, uint32_t key) {
    ind[0] = key;
    ind[1] = key >> 8;
    ind[2] = key >> 16;
    ind[3] = key >> 24;
  }

    // Add an IND record to the table
  // Return a pointer to the indirection key,
  // so it can be set later by the caller
  uint8_t* addIND() {
    if (numChunks == 5) nextBeat();
    uint8_t* ptr = currentRecord48();
    ptr[5] = 4 << 5;
    numChunks++;
    numRecords++;
    return ptr;
  }

 public:

  // A table holding encoded routing beats for each RAM
  Seq<uint8_t>** table;

  // Constructor
  ProgRouter() {
    // Currently we assume two RAMs per board
    assert(TinselDRAMsPerBoard == 2);
    // Initialise member variables
    prevInd = NULL;
    numBeats = 1;
    numChunks = numRecords = currentRAM = 0;
    // Allocate one sequence per RAM
    table = new Seq<uint8_t>* [TinselDRAMsPerBoard];
    // Initially each sequence is 32MB
    for (int i = 0; i < TinselDRAMsPerBoard; i++) {
      table[i] = new Seq<uint8_t> (1 << 15);
      // Allocate first beat
      table[i]->extendByAndZeroExtra(32);
    }
  }

  // Destructor
  ~ProgRouter() {
    for (int i = 0; i < TinselDRAMsPerBoard; i++) delete table[i];
    delete [] table;
  }

  // Generate a new key for the records added
  uint32_t genKey() {

    // Determine index of first beat in record sequence
    uint32_t index = table[currentRAM]->numElems - numBeats*32;
    // Determine final key length
    uint32_t finalKeyLen = prevInd ? 31 : numBeats;
    // Insert outstanding indirection, if there is one
    if (prevInd) {
      // Set previous indirection to latest block of beats
      uint32_t indKey = TinselPOLiteProgRouterBase +
        table[currentRAM]->numElems - (numBeats%31)*32;
      if (currentRAM) indKey |= 0x80000000;
      indKey |= (numBeats%31);
      setIND(prevInd, indKey); 
    }
    // Determine final key
    uint32_t key = TinselPOLiteProgRouterBase + index;
    if (currentRAM) key |= 0x80000000;
    key |= finalKeyLen;
    // Move to next beat
    nextBeat();
    numBeats = 1;
    prevInd = NULL;
    // Pick smaller RAM for next key
    currentRAM = table[0]->numElems < table[1]->numElems ? 0 : 1;
    return key;
  }

  // Add an MRM record to the table
  void addMRM(uint32_t mboxX, uint32_t mboxY,
                uint32_t threadsHigh, uint32_t threadsLow,
                  uint16_t localKey) {
    if (numChunks >= 4) nextBeat();
    uint8_t* ptr = currentRecord96();
    ptr[0] = threadsLow;
    ptr[1] = threadsLow >> 8;
    ptr[2] = threadsLow >> 16;
    ptr[3] = threadsLow >> 24;
    ptr[4] = threadsHigh;
    ptr[5] = threadsHigh >> 8;
    ptr[6] = threadsHigh >> 16;
    ptr[7] = threadsHigh >> 24;
    ptr[8] = localKey;
    ptr[9] = localKey >> 8;
    ptr[11] = (3 << 5) | (mboxY << 3) | (mboxX << 1);
    numChunks += 2;
    numRecords++;
  }

  // Add an RR record to the table
  void addRR(uint32_t dir, uint32_t key) {

    if (numChunks == 5) nextBeat();
    uint8_t* ptr = currentRecord48();
    ptr[0] = key;
    ptr[1] = key >> 8;
    ptr[2] = key >> 16;
    ptr[3] = key >> 24;
    ptr[5] = (2 << 5) | (dir << 3);
    numChunks++;
    numRecords++;
  }

  // Add a URM1 record to the table
  void addURM1(uint32_t mboxX, uint32_t mboxY,
                 uint32_t threadId, uint32_t key) {
    
    if (numChunks == 5) nextBeat();
    uint8_t* ptr = currentRecord48();
    ptr[0] = key;
    ptr[1] = key >> 8;
    ptr[2] = key >> 16;
    ptr[3] = key >> 24;
    ptr[4] = (threadId << 3);
    ptr[5] = (mboxY << 3) | (mboxX << 1) | (threadId >> 5);
    numChunks++;
    numRecords++;
  }
};

// ==================================
// Data type for routing destinations
// ==================================

enum PRoutingDestKind { PRDestKindURM1, PRDestKindMRM };

// URM1 routing destination
struct PRoutingDestURM1 {
  // Mailbox-local thread
  uint16_t threadId;
  // Thread-local routing key
  uint32_t key;
};

// MRM routing destination
struct PRoutingDestMRM {
  // Thread-local routing key
  uint16_t key;
  // Destination threads
  uint32_t threadMaskLow;
  uint32_t threadMaskHigh;
};

// Routing destination
struct PRoutingDest {
  PRoutingDestKind kind;
  // Destination mailbox
  uint32_t mbox;
  // URM1 or MRM destination
  union {
    PRoutingDestURM1 urm1;
    PRoutingDestMRM mrm;
  };
};

// Extract board X coord from routing dest
inline uint32_t destX(uint32_t mbox) {
  uint32_t x = mbox >> (TinselMailboxMeshXBits + TinselMailboxMeshYBits);
  return x & ((1<<TinselMeshXBits) - 1);
}

// Extract board Y coord from routing dest
inline uint32_t destY(uint32_t mbox) {
  uint32_t y = mbox >> (TinselMailboxMeshXBits +
                 TinselMailboxMeshYBits + TinselMeshXBits);
  return y & ((1<<TinselMeshYBits) - 1);
}

// Extract board-local mailbox X coord from routing dest
inline uint32_t destMboxX(uint32_t mbox) {
  return mbox & ((1<<TinselMailboxMeshXBits) - 1);
}

// Extract board-local mailbox Y coord from routing dest
inline uint32_t destMboxY(uint32_t mbox) {
  return (mbox >> TinselMailboxMeshXBits) &
           ((1<<TinselMailboxMeshYBits) - 1);
}

// ============================
// Mesh of programmable routers
// ============================

class ProgRouterMesh {
  // Board mesh dimensions
  uint32_t boardsX;
  uint32_t boardsY;

  std::recursive_mutex m_mutex;

  // 2D array of tables;
  ProgRouter** table;

 public:

  // Constructor
  ProgRouterMesh(uint32_t numBoardsX, uint32_t numBoardsY) {
    boardsX = numBoardsX;
    boardsY = numBoardsY;
    table = new ProgRouter* [numBoardsY];
    for (int y = 0; y < numBoardsY; y++)
      table[y] = new ProgRouter [numBoardsX];
  }

  // Add routing destinations from given sender board
  // Returns routing key
  uint32_t addDestsFromBoardXY(uint32_t senderX, uint32_t senderY,
                                 Seq<PRoutingDest>* dests) {
    if (dests->numElems == 0) return 0;

    // Categorise dests into local, N, S, E, and W groups
    Seq<PRoutingDest> local(dests->numElems);
    Seq<PRoutingDest> north(dests->numElems);
    Seq<PRoutingDest> south(dests->numElems);
    Seq<PRoutingDest> east(dests->numElems);
    Seq<PRoutingDest> west(dests->numElems);
    for (int i = 0; i < dests->numElems; i++) {
      PRoutingDest dest = dests->elems[i];
      uint32_t receiverX = destX(dest.mbox);
      uint32_t receiverY = destY(dest.mbox);
      if (receiverX < senderX) west.append(dest);
      else if (receiverX > senderX) east.append(dest);
      else if (receiverY < senderY) south.append(dest);
      else if (receiverY > senderY) north.append(dest);
      else local.append(dest);
    }

    std::lock_guard<std::recursive_mutex> lk(m_mutex);

    // Recurse on non-local groups and add RR records on return
    if (north.numElems > 0) {
      uint32_t key = addDestsFromBoardXY(senderX, senderY+1, &north);
      table[senderY][senderX].addRR(0, key);
    }
    if (south.numElems > 0) {
      uint32_t key = addDestsFromBoardXY(senderX, senderY-1, &south);
      table[senderY][senderX].addRR(1, key);
    }
    if (east.numElems > 0) {
      uint32_t key = addDestsFromBoardXY(senderX+1, senderY, &east);
      table[senderY][senderX].addRR(2, key);
    }
    if (west.numElems > 0) {
      uint32_t key = addDestsFromBoardXY(senderX-1, senderY, &west);
      table[senderY][senderX].addRR(3, key);
    }

    // Add local records
    for (int i = 0; i < local.numElems; i++) {
      PRoutingDest dest = local.elems[i];
      if (dest.kind == PRDestKindMRM) {
        table[senderY][senderX].addMRM(destMboxX(dest.mbox),
          destMboxY(dest.mbox), dest.mrm.threadMaskHigh,
          dest.mrm.threadMaskLow, dest.mrm.key);
      }
      else if (dest.kind == PRDestKindURM1) {
        table[senderY][senderX].addURM1(destMboxX(dest.mbox),
          destMboxY(dest.mbox), dest.urm1.threadId, dest.urm1.key);
      }
      else {
        fprintf(stderr, "ProgRouters.h: unknown routing record kind\n");
        exit(EXIT_FAILURE);
      }
    }

    return table[senderY][senderX].genKey();
  }

  // Add routing destinations from given global mailbox id
  uint32_t addDestsFromBoard(uint32_t mbox, Seq<PRoutingDest>* dests) {
    return addDestsFromBoardXY(destX(mbox), destY(mbox), dests);
  }

  // Write routing tables to memory via HostLink
  void write(HostLink* hostLink) {
    // Request to boot loader
    BootReq req;

    // Compute number of cores per DRAM
    const uint32_t coresPerDRAM = 1 <<
      (TinselLogCoresPerDCache + TinselLogDCachesPerDRAM);

    // Initialise write address for each routing table
    for (int y = 0; y < boardsY; y++) {
      for (int x = 0; x < boardsX; x++) {
        for (int i = 0; i < TinselDRAMsPerBoard; i++) {
          // Use one core to initialise each DRAM
          uint32_t dest = hostLink->toAddr(x, y, coresPerDRAM * i, 0);
          req.cmd = SetAddrCmd;
          req.numArgs = 1;
          req.args[0] = TinselPOLiteProgRouterBase;
          hostLink->send(dest, 1, &req);
          // Ensure space for an extra 32 bytes in each 
          // table so we don't have to check for overflow below
          // when consuming the tables in chunks of 12 bytes
          table[y][x].table[i]->ensureSpaceFor(32);
        }
      }
    }

    // Write each routing table
    bool allDone = false;
    uint32_t offset = 0;
    while (! allDone) {
      allDone = true;
      for (int y = 0; y < boardsY; y++) {
        for (int x = 0; x < boardsX; x++) {
          for (int i = 0; i < TinselDRAMsPerBoard; i++) {
            Seq<uint8_t>* seq = table[y][x].table[i];
            if (offset < seq->numElems) {
              uint32_t dest = hostLink->toAddr(x, y, coresPerDRAM * i, 0);
              uint8_t* base = &seq->elems[offset];
              allDone = false;
              req.cmd = StoreCmd;
              req.numArgs = 3;
              req.args[0] = ((uint32_t*) base)[0];
              req.args[1] = ((uint32_t*) base)[1];
              req.args[2] = ((uint32_t*) base)[2];
              hostLink->send(dest, 1, &req);
            }
          }
        }
      }
      offset += 12;
    }
  }

  // Destructor
  ~ProgRouterMesh() {
     for (int y = 0; y < boardsY; y++)
       delete [] table[y];
     delete [] table;
  }
};


#endif
