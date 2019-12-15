// SPDX-License-Identifier: BSD-2-Clause
#include "HostLink.h"
#include "DebugLink.h"
#include "MemFileReader.h"
#include "PowerLink.h"
#include "SocketUtils.h"

#include <boot.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <signal.h>

// Send buffer size (in bytes)
#define SEND_BUFFER_SIZE 131072

// Function to connect to a PCIeStream UNIX domain socket
static int connectToPCIeStream(const char* socketPath)
{
  // Create socket
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Make it non-blocking
  int opts = fcntl(sock, F_GETFL);
  fcntl(sock, F_SETFL, opts | O_NONBLOCK);

  // Connect socket
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  addr.sun_path[0] = '\0';
  strncpy(&addr.sun_path[1], socketPath, sizeof(addr.sun_path) - 2);
  int ret = connect(sock, (const struct sockaddr *) &addr,
                  sizeof(struct sockaddr_un));
  if (ret == -1) {
    fprintf(stderr, "Can't connect to PCIeStream daemon.\n"
                    "Either the daemon is not running or it is "
                    "being used by another process\n");
    exit(EXIT_FAILURE);
  }

  // Make it blocking again
  fcntl(sock, F_SETFL, opts);

  return sock;
}

// Internal constructor
void HostLink::constructor(uint32_t numBoxesX, uint32_t numBoxesY)
{
  if (numBoxesX > TinselBoxMeshXLen || numBoxesY > TinselBoxMeshYLen) {
    fprintf(stderr, "Number of boxes requested exceeds those available\n");
    exit(EXIT_FAILURE);
  }

  // Open lock file
  lockFile = open("/tmp/HostLink.lock", O_CREAT, 0444);
  if (lockFile == -1) {
    perror("Unable to open HostLink lock file");
    exit(EXIT_FAILURE);
  }

  // Acquire lock
  if (flock(lockFile, LOCK_EX | LOCK_NB) != 0) {
    perror("Failed to acquire HostLink lock");
    exit(EXIT_FAILURE);
  }

  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  #ifdef SIMULATE
    // Connect to simulator
    pcieLink = connectToPCIeStream(PCIESTREAM_SIM);
  #else
    // Connect to pciestreamd
    pcieLink = connectToPCIeStream(PCIESTREAM);
  #endif

  // Create DebugLink
  debugLink = new DebugLink(numBoxesX, numBoxesY);

  // Set board mesh dimensions
  meshXLen = debugLink->meshXLen;
  meshYLen = debugLink->meshYLen;

  // Allocate line buffers
  lineBuffer = new char**** [meshXLen];
  for (int x = 0; x < meshXLen; x++) {
    lineBuffer[x] = new char*** [meshYLen];
    for (int y = 0; y < meshYLen; y++) {
      lineBuffer[x][y] = new char** [TinselCoresPerBoard];
      for (int c = 0; c < TinselCoresPerBoard; c++) {
        lineBuffer[x][y][c] = new char* [TinselThreadsPerCore];
        for (int t = 0; t < TinselThreadsPerCore; t++) {
          lineBuffer[x][y][c][t] = new char [MaxLineLen];
        }
      }
    }
  }

  // Allocate and initialise line buffer lengths
  lineBufferLen = new int*** [meshXLen];
  for (int x = 0; x < meshXLen; x++) {
    lineBufferLen[x] = new int** [meshYLen];
    for (int y = 0; y < meshYLen; y++) {
      lineBufferLen[x][y] = new int* [TinselCoresPerBoard];
      for (int c = 0; c < TinselCoresPerBoard; c++) {
        lineBufferLen[x][y][c] = new int [TinselThreadsPerCore];
        for (int t = 0; t < TinselThreadsPerCore; t++)
          lineBufferLen[x][y][c][t] = 0;
      }
    }
  }

  // Initialise send buffer
  useSendBuffer = false;
  sendBuffer = new char [SEND_BUFFER_SIZE];
  sendBufferLen = 0;

  // Run the self test
  if (! powerOnSelfTest()) {
    fprintf(stderr, "Power-on self test failed.  Please try again.\n");
    exit(EXIT_FAILURE);
  }
}

HostLink::HostLink()
{
  char* str = getenv("HOSTLINK_BOXES_X");
  int x = str ? atoi(str) : 1;
  str = getenv("HOSTLINK_BOXES_Y");
  int y = str ? atoi(str) : 1;
  constructor(x, y);
}

HostLink::HostLink(uint32_t numBoxesX, uint32_t numBoxesY)
{
  constructor(numBoxesX, numBoxesY);
}

// Destructor
HostLink::~HostLink()
{
  // Free line buffers
  for (int x = 0; x < meshXLen; x++) {
    for (int y = 0; y < meshYLen; y++) {
      for (int c = 0; c < TinselCoresPerBoard; c++) {
        for (int t = 0; t < TinselThreadsPerCore; t++)
          delete [] lineBuffer[x][y][c][t];
        delete [] lineBuffer[x][y][c];
        delete [] lineBufferLen[x][y][c];
      }
      delete [] lineBuffer[x][y];
      delete [] lineBufferLen[x][y];
    }
    delete [] lineBuffer[x];
    delete [] lineBufferLen[x];
  }
  delete [] lineBuffer;
  delete [] lineBufferLen;

  // Free send buffer
  delete [] sendBuffer;

  // Close debug link
  delete debugLink;

  // Close connection to the PCIe stream daemon
  close(pcieLink);

  // Release HostLink lock
  if (flock(lockFile, LOCK_UN) != 0) {
    perror("Failed to release HostLink lock");
  }
  close(lockFile);
}

// Address construction
uint32_t HostLink::toAddr(uint32_t meshX, uint32_t meshY,
             uint32_t coreId, uint32_t threadId)
{
  uint32_t addr;
  addr = meshY;
  addr = (addr << TinselMeshXBits) | meshX;
  addr = (addr << TinselLogCoresPerBoard) | coreId;
  addr = (addr << TinselLogThreadsPerCore) | threadId;
  return addr;
}

// Address deconstruction
void HostLink::fromAddr(uint32_t addr, uint32_t* meshX, uint32_t* meshY,
         uint32_t* coreId, uint32_t* threadId)
{
  *threadId = addr % (1 << TinselLogThreadsPerCore);
  addr >>= TinselLogThreadsPerCore;

  *coreId = addr % (1 << TinselLogCoresPerBoard);
  addr >>= TinselLogCoresPerBoard;

  *meshX = addr % (1 << TinselMeshXBits);
  addr >>= TinselMeshXBits;

  *meshY = addr;
}

// Inject a message via PCIe (blocking by default)
bool HostLink::send(uint32_t dest, void* payload, bool block)
{
  assert(useSendBuffer ? block : true);

  if (useSendBuffer) {
    // Flush the buffer when we run out of space
    if ((sendBufferLen+16+TinselBytesPerMsg) >= SEND_BUFFER_SIZE) flush();

    // Message buffer
    uint32_t* buffer = (uint32_t*) &sendBuffer[sendBufferLen];

    // Fill in the message header
    // (See DE5BridgeTop.bsv for details)
    buffer[0] = dest;
    buffer[1] = TinselWordsPerMsg; // Message size in 32-bit words

    // Fill in message payload
    memcpy(&buffer[4], payload, TinselBytesPerMsg);

    // Update buffer
    sendBufferLen += 16 + TinselBytesPerMsg;

    return true;
  }
  else {
    assert(sendBufferLen == 0);

    // Message buffer
    uint32_t buffer[4 + TinselWordsPerMsg];

    // Fill in the message header
    // (See DE5BridgeTop.bsv for details)
    buffer[0] = dest;
    buffer[1] = TinselWordsPerMsg; // Message size in 32-bit words

    // Fill in message payload
    memcpy(&buffer[4], payload, TinselBytesPerMsg);

    // Total bytes to send, including header
    int totalBytes = 16+TinselBytesPerMsg;

    // Write to the socket
    if (block) {
      socketBlockingPut(pcieLink, (char*) buffer, totalBytes);
      return true;
    }
    else {
      return socketPut(pcieLink, (char*) buffer, totalBytes) == 1;
    }
  }
}

// Flush the send buffer
void HostLink::flush()
{
  assert(useSendBuffer);
  if (sendBufferLen > 0) {
    socketBlockingPut(pcieLink, sendBuffer, sendBufferLen);
    sendBufferLen = 0;
  }
}

// Try to send a message (non-blocking, returns true on success)
bool HostLink::trySend(uint32_t dest, void* msg)
{
  return send(dest, msg, false);
}

// Receive a message via PCIe (blocking)
void HostLink::recv(void* msg)
{
  int numBytes = 1 << TinselLogBytesPerMsg;
  socketBlockingGet(pcieLink, (char*) msg, numBytes);
}

// Receive a message (blocking), given size of message in bytes
void HostLink::recvMsg(void* msg, uint32_t numBytes)
{
  // Number of padding bytes that need to be received but not stored
  int paddingBytes = (1 << TinselLogBytesPerMsg) - numBytes;

  // Fill message
  uint8_t* ptr = (uint8_t*) msg;
  socketBlockingGet(pcieLink, (char*) ptr, numBytes);

  // Discard padding bytes
  uint8_t padding[1 << TinselLogBytesPerMsg];
  socketBlockingGet(pcieLink, (char*) padding, paddingBytes);
}

// Receive multiple messages (blocking)
void HostLink::recvBulk(int numMsgs, void* msgs)
{
  int numBytes = numMsgs * (1 << TinselLogBytesPerMsg);
  socketBlockingGet(pcieLink, (char*) msgs, numBytes);
}

// Receive multiple messages (blocking), given size of each message
void HostLink::recvMsgs(int numMsgs, int msgSize, void* msgs)
{
  int numBytes = numMsgs * (1 << TinselLogBytesPerMsg);
  uint8_t* buffer = new uint8_t [numBytes];
  uint8_t* ptr = (uint8_t*) msgs;
  socketBlockingGet(pcieLink, (char*) buffer, numBytes);
  for (int i = 0; i < numMsgs; i++)
    memcpy(&ptr[i*msgSize], &buffer[i*(1<<TinselLogBytesPerMsg)], msgSize);
  delete [] buffer;
}

// Can receive a message without blocking?
bool HostLink::canRecv()
{
  return socketCanGet(pcieLink);
}

// Load application code and data onto the mesh
void HostLink::boot(const char* codeFilename, const char* dataFilename)
{
  MemFileReader code(codeFilename);
  MemFileReader data(dataFilename);

  // Request to boot loader
  BootReq req;

  // Total number of cores
  const uint32_t numCores =
    (meshXLen*meshYLen) << TinselLogCoresPerBoard;

  // Step 1: load code into instruction memory
  // -----------------------------------------

  uint32_t addrReg = 0xffffffff;
  uint32_t addr, word;
  while (code.getWord(&addr, &word)) {
    // Send instruction to each core
    for (int x = 0; x < meshXLen; x++) {
      for (int y = 0; y < meshYLen; y++) {
        for (int i = 0; i < (1 << TinselLogCoresPerBoard); i++) {
          uint32_t dest = toAddr(x, y, i, 0);
          if (addr != addrReg) {
            req.cmd = SetAddrCmd;
            req.numArgs = 1;
            req.args[0] = addr;
            send(dest, &req);
          }
          req.cmd = WriteInstrCmd;
          req.numArgs = 1;
          req.args[0] = word;
          send(dest, &req);
        }
      }
    }
    addrReg = addr + 4;
  }

  // Step 2: initialise data memory
  // ------------------------------

  // Compute number of cores per DRAM
  const uint32_t coresPerDRAM = 1 <<
    (TinselLogCoresPerDCache + TinselLogDCachesPerDRAM);

  // Write data to DRAMs
  addrReg = 0xffffffff;
  while (data.getWord(&addr, &word)) {
    for (int x = 0; x < meshXLen; x++) {
      for (int y = 0; y < meshYLen; y++) {
        for (int i = 0; i < TinselDRAMsPerBoard; i++) {
          // Use one core to initialise each DRAM
          uint32_t dest = toAddr(x, y, coresPerDRAM * i, 0);
          if (addr != addrReg) {
            req.cmd = SetAddrCmd;
            req.numArgs = 1;
            req.args[0] = addr;
            send(dest, &req);
          }
          req.cmd = StoreCmd;
          req.numArgs = 1;
          req.args[0] = word;
          send(dest, &req);
        }
      }
    }
    addrReg = addr + 4;
  }

  // Step 3: start cores
  // -------------------

  // Send start command
  uint32_t started = 0;
  uint32_t msg[1 << TinselLogWordsPerMsg];
  for (int x = 0; x < meshXLen; x++) {
    for (int y = 0; y < meshYLen; y++) {
      for (int i = 0; i < (1 << TinselLogCoresPerBoard); i++) {
        uint32_t dest = toAddr(x, y, i, 0);
        req.cmd = StartCmd;
        req.args[0] = (1<<TinselLogThreadsPerCore)-1;
        while (1) {
          bool ok = trySend(dest, &req);
          if (canRecv()) {
            recv(msg);
            started++;
          }
          if (ok) break;
        }
      }
    }
  }

  // Wait for all start responses
  while (started < numCores) {
    recv(msg);
    started++;
  }
}

// Trigger to start application execution
void HostLink::go()
{
  for (int x = 0; x < meshXLen; x++) {
    for (int y = 0; y < meshYLen; y++) {
      debugLink->setBroadcastDest(x, y, 0);
      debugLink->put(x, y, 0);
    }
  }
}

// Load instructions into given core's instruction memory
void HostLink::loadInstrsOntoCore(const char* codeFilename,
       uint32_t meshX, uint32_t meshY, uint32_t coreId)
{
  // Code file
  MemFileReader code(codeFilename);

  // Load loop
  BootReq req;
  uint32_t addrReg = 0xffffffff;
  uint32_t addr, word;
  uint32_t dest = toAddr(meshX, meshY, coreId, 0);
  while (code.getWord(&addr, &word)) {
    // Write instruction
    if (addr != addrReg) {
      req.cmd = SetAddrCmd;
      req.numArgs = 1;
      req.args[0] = addr;
      send(dest, &req);
    }
    req.cmd = WriteInstrCmd;
    req.numArgs = 1;
    req.args[0] = word;
    send(dest, &req);
    addrReg = addr + 4;
  }
}

// Load data via given core on given board
void HostLink::loadDataViaCore(const char* dataFilename,
        uint32_t meshX, uint32_t meshY, uint32_t coreId)
{
  MemFileReader data(dataFilename);

  // Write data to DRAM
  BootReq req;
  uint32_t addrReg = 0xffffffff;
  uint32_t addr, word;
  uint32_t dest = toAddr(meshX, meshY, coreId, 0);
  while (data.getWord(&addr, &word)) {
    // Write data
    if (addr != addrReg) {
      req.cmd = SetAddrCmd;
      req.numArgs = 1;
      req.args[0] = addr;
      send(dest, &req);
    }
    req.cmd = StoreCmd;
    req.numArgs = 1;
    req.args[0] = word;
    send(dest, &req);
    addrReg = addr + 4;
  }
}

// Start given number of threads on given core
void HostLink::startOne(uint32_t meshX, uint32_t meshY,
       uint32_t coreId, uint32_t numThreads)
{
  assert(numThreads > 0 && numThreads <= TinselThreadsPerCore);

  BootReq req;
  uint32_t dest = toAddr(meshX, meshY, coreId, 0);

  // Send start command
  req.cmd = StartCmd;
  req.args[0] = numThreads-1;
  send(dest, &req);

  // Wait for start response
  uint32_t msg[1 << TinselLogWordsPerMsg];
  recv(msg);
}

// Trigger application execution on all started threads on given core
void HostLink::goOne(uint32_t meshX, uint32_t meshY, uint32_t coreId)
{
  debugLink->setDest(meshX, meshY, coreId, 0);
  debugLink->put(meshX, meshY, 0);
}

// Set address for remote memory access on given board via given core
// (This address is auto-incremented on loads and stores)
void HostLink::setAddr(uint32_t meshX, uint32_t meshY,
                       uint32_t coreId, uint32_t addr)
{
  BootReq req;
  req.cmd = SetAddrCmd;
  req.numArgs = 1;
  req.args[0] = addr;
  send(toAddr(meshX, meshY, coreId, 0), &req);
}

// Store words to remote memory on a given board via given core
void HostLink::store(uint32_t meshX, uint32_t meshY,
                     uint32_t coreId, uint32_t numWords, uint32_t* data)
{
  BootReq req;
  req.cmd = StoreCmd;
  while (numWords > 0) {
    uint32_t sendWords = numWords > (TinselWordsPerMsg-1) ?
      (TinselWordsPerMsg-1) : numWords;
    numWords = numWords - sendWords;
    req.numArgs = sendWords;
    for (uint32_t i = 0; i < sendWords; i++) req.args[i] = data[i];
    send(toAddr(meshX, meshY, coreId, 0), &req);
  }
}

// Power-on self test
bool HostLink::powerOnSelfTest()
{
  const double timeout = 3.0;

  // Need to check that we get a response within a given time
  struct timeval start, finish, diff;

  // Boot request to load data from memory
  // (The test involves reading from QDRII+ SRAMs on each board)
  BootReq req;
  req.cmd = LoadCmd;
  req.numArgs = 1;
  req.args[0] = 1;

  // Message buffer to store responses
  uint32_t msg[1 << TinselLogWordsPerMsg];

  // Count number of responses received
  int count = 0;

  // Send request and consume responses
  for (int ram = 1; ram <= 2; ram++) {
    for (int y = 0; y < meshYLen; y++) {
      for (int x = 0; x < meshXLen; x++) {
        // Request a word from SRAM
        uint32_t addr = ram << TinselLogBytesPerSRAM;
        setAddr(x, y, 0, addr);
        gettimeofday(&start, NULL);
        while (1) {
          bool ok = trySend(toAddr(x, y, 0, 0), &req);
          if (canRecv()) {
            recv(msg);
            count++;
          }
          if (ok) break;
          gettimeofday(&finish, NULL);
          timersub(&finish, &start, &diff);
          double duration = (double) diff.tv_sec +
                            (double) diff.tv_usec / 1000000.0;
          if (duration > timeout) return false;
        }
      }
    }
  }

  // Consume remaining responses
  gettimeofday(&start, NULL);
  while (count < (2*meshXLen*meshYLen)) {
    if (canRecv()) {
      recv(msg);
      count++;
      gettimeofday(&start, NULL);
    }
    gettimeofday(&finish, NULL);
    timersub(&finish, &start, &diff);
    double duration = (double) diff.tv_sec +
                      (double) diff.tv_usec / 1000000.0;
    if (duration > timeout) return false;
  }

  return true;
}

// Redirect UART StdOut to given file
// Increment line count when appropriate
// Returns false when no data has been emitted
bool HostLink::pollStdOut(FILE* outFile, uint32_t* lineCount)
{
  bool got = false;
  while (debugLink->canGet()) {
    // Receive byte
    uint8_t byte;
    uint32_t x, y, c, t;
    debugLink->get(&x, &y, &c, &t, &byte);
    got = true;

    // Update line buffer & display on newline or buffer-full
    int len = lineBufferLen[x][y][c][t];
    if (byte == '\n' || len == MaxLineLen-1) {
      if (lineCount != NULL) (*lineCount)++;
      lineBuffer[x][y][c][t][len] = '\0';
      fprintf(outFile, "%d:%d:%d:%d: %s\n", x, y, c, t,
        lineBuffer[x][y][c][t]);
      lineBufferLen[x][y][c][t] = len = 0;
    }
    if (byte != '\n') {
      lineBuffer[x][y][c][t][len] = byte;
      lineBufferLen[x][y][c][t]++;
    }
  }
  return got;
}

// Receive StdOut byte streams and append to file (non-blocking)
bool HostLink::pollStdOut(FILE* outFile)
{
  return pollStdOut(outFile, NULL);
}

// Redirect UART StdOut to stdout
// Returns false when no data has been emitted
bool HostLink::pollStdOut()
{
  return pollStdOut(stdout);
}

// Redirect UART StdOut to given file (blocking function, never terminates)
void HostLink::dumpStdOut(FILE* outFile)
{
  for (;;) {
    bool ok = pollStdOut(outFile);
    if (!ok) usleep(10000);
  }
}

// Receive lines from StdOut byte streams and append to file (blocking)
void HostLink::dumpStdOut(FILE* outFile, uint32_t lines)
{
  uint32_t count = 0;
  while (count < lines) {
    bool ok = pollStdOut(outFile, &count);
    if (!ok) usleep(10000);
  }
}

// Display UART StdOut (blocking function, never terminates)
void HostLink::dumpStdOut()
{
  dumpStdOut(stdout);
}
