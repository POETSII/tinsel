// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HOSTLINK_H_
#define _HOSTLINK_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <config.h>
#include <DebugLink.h>

// Max line length for line-buffered UART StdOut capture
#define MaxLineLen 128

// Connections to PCIeStream
#define PCIESTREAM      "pciestream"
#define PCIESTREAM_SIM  "tinsel.b-1.1"

// HostLink parameters
struct HostLinkParams {
  uint32_t numBoxesX;
  uint32_t numBoxesY;
  bool useExtraSendSlot;

  // Used to allow retries when connecting to the socket. When performing rapid sweeps,
  // it is quite common for the first attempt in the next process to fail.
  int max_connection_attempts;
  HostLinkParams(): max_connection_attempts(5){}
};

class HostLink {
  // Lock file for acquring exclusive access to PCIeStream
  int lockFile;

  // File descriptor for link to PCIeStream
  int pcieLink;

  // Line buffers for JTAG UART StdOut
  // Max line length defined by MaxLineLen
  // Indexed by (board X, board Y, core, thread)
  char***** lineBuffer;
  int**** lineBufferLen;

  // Send buffer, for bulk sending over PCIe
  char* sendBuffer;
  int sendBufferLen;

  // Request an extra send slot when bringing up Tinsel FPGAs
  bool useExtraSendSlot;

  // Internal constructor
  void constructor(HostLinkParams params);

  // Internal helper for sending messages
  bool sendHelper(uint32_t dest, uint32_t numFlits, void* payload,
         bool block, uint32_t key);
 public:
  // Dimensions of board mesh
  int meshXLen;
  int meshYLen;

  // Constructors
  HostLink();
  HostLink(uint32_t numBoxesX, uint32_t numBoxesY);
  HostLink(HostLinkParams params);

  // Destructor
  ~HostLink();
 
  // Power-on self test
  bool powerOnSelfTest();

  // Debug links
  // -----------

  // DebugLink (access to FPGAs via their JTAG UARTs)
  DebugLink* debugLink;

  // Send and receive messages over PCIe
  // -----------------------------------

  // Send a message (blocking by default)
  bool send(uint32_t dest, uint32_t numFlits, void* msg, bool block = true);

  // Try to send a message (non-blocking, returns true on success)
  bool trySend(uint32_t dest, uint32_t numFlits, void* msg);

  // Send a message using routing key (blocking by default)
  bool keySend(uint32_t key, uint32_t numFlits, void* msg, bool block = true);

  // Try to send using routing key (non-blocking, returns true on success)
  bool keyTrySend(uint32_t key, uint32_t numFlits, void* msg);

  // Receive a max-sized message (blocking)
  void recv(void* msg);

  // Can receive a flit without blocking?
  bool canRecv();

  // Receive a message (blocking), given size of message in bytes
  void recvMsg(void* msg, uint32_t numBytes);

  // Bulk send and receive
  // ---------------------

  // Receive multiple max-sized messages (blocking)
  void recvBulk(int numMsgs, void* msgs);

  // Receive multiple messages (blocking), given size of each message
  void recvMsgs(int numMsgs, int msgSize, void* msgs);

  // When enabled, use buffer for sending messages, permitting bulk writes
  // The buffer must be flushed to ensure data is sent
  // Currently, only blocking sends are supported in this mode
  bool useSendBuffer;

  // Flush the send buffer (when send buffering is enabled)
  void flush();

  // Address construction/deconstruction
  // -----------------------------------

  // Address construction
  uint32_t toAddr(uint32_t meshX, uint32_t meshY,
             uint32_t coreId, uint32_t threadId);

  // Address deconstruction
  void fromAddr(uint32_t addr, uint32_t* meshX, uint32_t* meshY,
         uint32_t* coreId, uint32_t* threadId);

  // Assuming the boot loader is running on the cores
  // ------------------------------------------------
  //
  // (Only thread 0 on each core is active when the boot loader is running)

  // Load application code and data onto the mesh
  void loadAll(const char* codeFilename, const char* dataFilename);

  // ... and start
  void boot(const char* codeFilename, const char* dataFilename);

  // Trigger to start application execution
  void go();

  // Set address for remote memory access to given board via given core
  // (This address is auto-incremented on loads and stores)
  void setAddr(uint32_t meshX, uint32_t meshY,
               uint32_t coreId, uint32_t addr);

  // Store words to remote memory on given board via given core
  void store(uint32_t meshX, uint32_t meshY,
             uint32_t coreId, uint32_t numWords, uint32_t* data);

  // Finer-grained control over application loading and execution
  // ------------------------------------------------------------

  // Load instructions into given core's instruction memory
  void loadInstrsOntoCore(const char* codeFilename,
         uint32_t meshX, uint32_t meshY, uint32_t coreId);

  // Load data via given core on given board
  void loadDataViaCore(const char* dataFilename,
        uint32_t meshX, uint32_t meshY, uint32_t coreId);

  // Start given number of threads on given core
  void startOne(uint32_t meshX, uint32_t meshY,
         uint32_t coreId, uint32_t numThreads);

  // Start all threads on all cores
  void startAll();

  // Trigger application execution on all started threads on given core
  void goOne(uint32_t meshX, uint32_t meshY, uint32_t coreId);

  // Line-buffered StdOut console
  // ----------------------------

  // Receive StdOut byte streams and append to file (non-blocking)
  bool pollStdOut(FILE* outFile);

  // Receive StdOut byte streams and append to file (non-blocking)
  // and increment line count
  bool pollStdOut(FILE* outFile, uint32_t* lineCount);

  // Receive StdOut byte streams and display on stdout (non-blocking)
  bool pollStdOut();

  // Receive StdOut byte streams and append to file (non-terminating)
  void dumpStdOut(FILE* outFile);

  // Receive a number of lines from StdOut byte streams
  // and append to file (blocking)
  void dumpStdOut(FILE* outFile, uint32_t lines);

  // Receive StdOut byte streams and display on stdout (non-terminating)
  void dumpStdOut();
};

#endif
