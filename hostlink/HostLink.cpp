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

#include <set>


// Send buffer size (in flits)
#define SEND_BUFFER_SIZE 8192

bool HostLink::flushcore(uint32_t dest) {
  // ensure all messages have reached the core
  // we pick a rand int32, and wait until it's echo'd by the core
  // assuming in-order msg delivery, this ensures we've sent eveything.
  usleep(10000);
  // debugprintf(debugLink);
  uint32_t cookie = rand();
  BootReq req;
  uint32_t msg[1 << TinselLogWordsPerMsg];

  req.cmd = FlushCmd;
  req.numArgs = 1;
  req.args[0] = cookie;
  printf("sending flush alligner with cookie %i\n", cookie);
  send(dest, 1, &req);
  usleep(10000);

  int iteration = 0;

  do {

    // don't block on DebugLink
    // debugprintf(debugLink);

    // process flits from the core
    while (canRecv()) {
      recv(msg);
      printf("0x%04X sent: ", dest);
      for (int j=0; j<1 << TinselLogWordsPerMsg; j++) printf("msg[%i]=%i ", j, msg[j]);
      printf("\n");
      usleep(10000);
      if (msg[0] == cookie) {
        printf("0x%04X synced after %i flits\n", dest, iteration+1);
        uint32_t recv_meshX, recv_meshY, recv_coreId, recv_threadId;
        fromAddr(msg[1], &recv_meshX, &recv_meshY, &recv_coreId, &recv_threadId);
        printf("sync msg from Mx: %i My: %i core: %i thread: %i\n", recv_meshX, recv_meshY, recv_coreId, recv_threadId);

        return 1; // done.
      } else {
        printf("not cookie, ignoring.\n");
      }
    }

    req.cmd = NOPCmd;
    req.numArgs = 0;
    send(dest, 1, &req);
    printf(".");
    fflush(0);
    iteration++;
    usleep(10000);
  } while (iteration < 3);
  printf("0x%04X NOT synced after %i flits\n", dest, iteration+1);

  return 0;

}


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
  printf("connecting to pciestream socket %s\n", socketPath);
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
void HostLink::constructor(HostLinkParams p)
{
  useExtraSendSlot = p.useExtraSendSlot;

  if (p.numBoxesX > TinselBoxMeshXLen || p.numBoxesY > TinselBoxMeshYLen) {
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
  printf("connected to PCIe stream.\n");

  // Create DebugLink
  DebugLinkParams debugLinkParams;
  debugLinkParams.numBoxesX = p.numBoxesX;
  debugLinkParams.numBoxesY = p.numBoxesY;
  debugLinkParams.useExtraSendSlot = p.useExtraSendSlot;
  debugLinkParams.max_connection_attempts=p.max_connection_attempts;
  debugLink = new DebugLink(debugLinkParams);
  printf("connected to DebugLink.\n");

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
  sendBuffer = new char [(1<<TinselLogBytesPerFlit) * SEND_BUFFER_SIZE];
  // avoids (correct) warnings by valgrind about passing un-init memory to syscall. In
  // cases seen this is fine, as it is un-init padding being passed through to fill in
  // spaces in tables for alignment purposes.
  memset(sendBuffer, 0, (1<<TinselLogBytesPerFlit) * SEND_BUFFER_SIZE);
  sendBufferLen = 0;

  // Run the self test
  int retry=0;
  for (retry=1; retry>0; retry--) {
    if (powerOnSelfTest()) break;
  }
  if (retry == 0) {
    fprintf(stderr, "Power-on self test failed.  Please try again.\n");
    exit(EXIT_FAILURE);
  } else {
    printf("hostlink POST passed.\n");
  }
}

HostLink::HostLink()
{
  char* str = getenv("HOSTLINK_BOXES_X");
  int x = str ? atoi(str) : 1;
  str = getenv("HOSTLINK_BOXES_Y");
  int y = str ? atoi(str) : 1;
  HostLinkParams params;
  params.numBoxesX = x;
  params.numBoxesY = y;
  params.useExtraSendSlot = false;
  constructor(params);
}

HostLink::HostLink(uint32_t numBoxesX, uint32_t numBoxesY)
{
  HostLinkParams params;
  params.numBoxesX = numBoxesX;
  params.numBoxesY = numBoxesY;
  params.useExtraSendSlot = false;
  constructor(params);
}

HostLink::HostLink(HostLinkParams params)
{
  constructor(params);
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
uint32_t tinselToAddr(
         uint32_t boardX, uint32_t boardY,
           uint32_t tileX, uint32_t tileY,
             uint32_t coreId, uint32_t threadId)
{
  assert(boardX < (1<<TinselMeshXBits));
  assert(boardY < (1<<TinselMeshYBits));
  assert(tileY < (1<<TinselMailboxMeshYBits));
  assert(tileX < (1<<TinselMailboxMeshXBits));
  assert(coreId < (1<<TinselLogCoresPerMailbox));
  assert(threadId < (1<<TinselLogThreadsPerCore));
  uint32_t addr;
  addr = boardY;
  addr = (addr << TinselMeshXBits) | boardX;
  addr = (addr << TinselMailboxMeshYBits) | tileY;
  addr = (addr << TinselMailboxMeshXBits) | tileX;
  addr = (addr << TinselLogCoresPerMailbox) | coreId;
  addr = (addr << TinselLogThreadsPerCore) | threadId;
  return addr;
}


// Address construction
// uint32_t HostLink::toAddr(uint32_t meshX, uint32_t meshY,
//              uint32_t coreId, uint32_t threadId)
// {
//   uint32_t addr;
//   addr = tinselToAddr(meshX, meshY,
//                       coreId / (TinselMailboxMeshYLen* 1<<TinselLogCoresPerMailbox),
//                       (coreId / 1<<TinselLogCoresPerMailbox) % TinselMailboxMeshYLen,
//                       coreId % 1<<TinselLogCoresPerMailbox,
//                     threadId);
//   return addr;
// }

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

// Internal helper for sending messages
bool HostLink::sendHelper(uint32_t dest, uint32_t numFlits, void* payload,
       bool block, uint32_t key)
{
  assert(useSendBuffer ? block : true);

  // Ensure that MaxFlitsPerMsg is not violated
  assert(numFlits > 0 && numFlits <= TinselMaxFlitsPerMsg);

  // We assume that message flits are 128 bits
  // (Because PCIeStream currently has this assumption)
  assert(TinselLogBytesPerFlit == 4);

  if (useSendBuffer) {
    // Flush the buffer when we run out of space
    if ((sendBufferLen + numFlits + 1) >= SEND_BUFFER_SIZE) flush();

    // Message buffer
    uint32_t* buffer = (uint32_t*) &sendBuffer[16*sendBufferLen];

    // Fill in the message header
    // (See DE5BridgeTop.bsv for details)
    buffer[0] = dest;
    buffer[1] = 0;
    buffer[2] = (numFlits-1) << 24;
    buffer[3] = key;

    // Fill in message payload
    memcpy(&buffer[4], payload, numFlits*16);

    // Update buffer
    sendBufferLen += 1 + numFlits;

    return true;
  }
  else {
    assert(sendBufferLen == 0);

    // Message buffer
    uint32_t buffer[4*(TinselMaxFlitsPerMsg+1)];

    // Fill in the message header
    // (See DE5BridgeTop.bsv for details)
    buffer[0] = dest;
    buffer[1] = 0;
    buffer[2] = (numFlits-1) << 24;
    buffer[3] = 0;

    // Bytes in payload
    int payloadBytes = numFlits*16;

    // Fill in message payload
    memcpy(&buffer[4], payload, payloadBytes);

    // Total bytes to send, including header
    int totalBytes = 16+payloadBytes;

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


// Inject a message via PCIe (blocking by default)
bool HostLink::send(uint32_t dest, uint32_t numFlits, void* msg, bool block)
{
  return sendHelper(dest, numFlits, msg, block, 0);
}

// Flush the send buffer
void HostLink::flush()
{
  assert(useSendBuffer);
  if (sendBufferLen > 0) {
    socketBlockingPut(pcieLink, sendBuffer, sendBufferLen * 16);
    sendBufferLen = 0;
  }
}

// Try to send a message (non-blocking, returns true on success)
bool HostLink::trySend(uint32_t dest, uint32_t numFlits, void* msg)
{
  return sendHelper(dest, numFlits, msg, false, 0);
}

// Send a message using routing key (blocking by default)
bool HostLink::keySend(uint32_t key, uint32_t numFlits,
       void* msg, bool block)
{
  uint32_t useRoutingKey = 1 << (
    TinselLogThreadsPerCore + TinselLogCoresPerMailbox +
    TinselMailboxMeshXBits + TinselMailboxMeshYBits +
    TinselMeshXBits + TinselMeshYBits + 2);
  return sendHelper(useRoutingKey, numFlits, msg, block, key);
}

// Try to send using routing key (non-blocking, returns true on success)
bool HostLink::keyTrySend(uint32_t key, uint32_t numFlits, void* msg)
{
  uint32_t useRoutingKey = 1 << (
    TinselLogThreadsPerCore + TinselLogCoresPerMailbox +
    TinselMailboxMeshXBits + TinselMailboxMeshYBits +
    TinselMeshXBits + TinselMeshYBits + 2);
  return sendHelper(useRoutingKey, numFlits, msg, false, key);
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

// Can receive a flit without blocking?
bool HostLink::canRecv()
{
  return socketCanGet(pcieLink);
}

bool HostLink::test(uint32_t dest) {
  // clobbers core ADDR reg!
  return 1; // skip for simulation

  uint32_t load_reply_buf[1 << TinselLogWordsPerMsg];
  BootReq req;
  memset(&req, 0, sizeof(BootReq)); // Keep valgrind happy about un-init bytes.

  req.cmd = SetAddrCmd;
  req.numArgs = 1;
  req.args[0] = 16;
  send(dest, 1, &req);

  req.cmd = LoadCmd;
  req.numArgs = 1;
  req.args[0] = 4;
  send(dest, 1, &req, true);

  bool got = 0;
  for (int rpt=0; rpt<1000; rpt++) {
    if (!canRecv()) {
      got = 1;
      break;
    }
    usleep(100);
  }
  if (got) recv(load_reply_buf); // discard the single flit we generated
  return got;
}

bool HostLink::test(uint32_t dest, uint32_t addr, uint32_t* value) {
  // clobbers core ADDR reg!
  return 1; // skip for simulation

  uint32_t load_reply_buf[1 << TinselLogWordsPerMsg];
  BootReq req;
  memset(&req, 0, sizeof(BootReq)); // Keep valgrind happy about un-init bytes.

  req.cmd = SetAddrCmd;
  req.numArgs = 1;
  req.args[0] = addr;
  send(dest, 1, &req);

  req.cmd = LoadCmd;
  req.numArgs = 1;
  req.args[0] = 4;
  send(dest, 1, &req, true);

  bool got = 0;
  for (int rpt=0; rpt<1000; rpt++) {
    if (!canRecv()) {
      got = 1;
      break;
    }
    usleep(100);
  }


  if (!got) {
    // printf("[hostlink::LoadAll] failed to get reply for datamem read req addr %i core %i:%i:%i\n", addr, x, y, i);
    return false;
  }

  recv(load_reply_buf);
  if (load_reply_buf[0] != *value) {
     // printf("[hostlink::LoadAll] data mismatch at addr %i core %i:%i:%i: expected %04X, got %04X\n", addr, x, y, i, word, load_reply_buf[0]);
     *value = load_reply_buf[0];
     return false;
  }
  return true;
}


// Load application code and data onto the mesh
void HostLink::loadAll(const char* codeFilename, const char* dataFilename)
{
  MemFileReader code(codeFilename);
  MemFileReader data(dataFilename);

  // Request to boot loader
  BootReq req;
  memset(&req, 0, sizeof(BootReq)); // Keep valgrind happy about un-init bytes.


  // Step 1: load code into instruction memory
  // -----------------------------------------

  uint32_t addrReg = 0xffffffff;
  uint32_t addr, word;
  uint32_t load_reply_buf[1 << TinselLogWordsPerMsg];

  while (code.getWord(&addr, &word)) {
    // Send instruction to each core
    for (int x = 0; x < meshXLen; x++) {
      for (int y = 0; y < meshYLen; y++) {
        for (int i = 0; i < (1 << TinselLogCoresPerBoard); i++) {
          uint32_t dest = toAddr(x, y, i, 0);
          if (!test(dest)) {
            printf("[hostlink::LoadAll] failed to get reply for datamem read BEFORE code load addr %i core %i:%i:%i during code loading\n", addr, x, y, i);
          }

          if (addr != addrReg || true) {
            req.cmd = SetAddrCmd;
            req.numArgs = 1;
            req.args[0] = addr;
            send(dest, 1, &req);
          }
          if (addr == 0) {
            printf("[hostlink::LoadAll] set addr to 0\n");
          }

          req.cmd = WriteInstrCmd;
          req.numArgs = 1;
          req.args[0] = word;
          send(dest, 1, &req);
          // printf("sent word %d\n", addr);

          // check the core is still responding.
          if (!test(dest)) {
            printf("[hostlink::LoadAll] failed to get reply for datamem read AFTER code load addr %i core %i:%i:%i during code loading\n", addr, x, y, i);
          }

        }
      }
    }
    addrReg = addr + 4;
    printf("[hostlink::LoadAll::instrs] %i\n", addr);
  }
  printf("[hostlink::LoadAll] written code memory\n");

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
        // for (int i = 0; i < TinselDRAMsPerBoard; i++) {
        for (int i = 0; i < TinselCoresPerBoard; i++) {
          // Use one core to initialise each DRAM
          // uint32_t dest = toAddr(x, y, coresPerDRAM * i, 0);
          // just until I understand the core<>dram mapping, write to all cores
          // data mem.
          uint32_t dest = toAddr(x, y, i, 0);
          if (addr != addrReg || true) {
            req.cmd = SetAddrCmd;
            req.numArgs = 1;
            req.args[0] = addr;
            send(dest, 1, &req);
          }
          req.cmd = StoreCmd;
          req.numArgs = 1;
          req.args[0] = word;
          send(dest, 1, &req);

          // inline check
          uint32_t word_test = word;
          if (!test(dest, addr, &word_test)) {
            printf("[hostlink::LoadAll] failed to verify for datamem read during data loading %i core %i:%i:%i during code loading. correct %x recv %x\n", addr, x, y, i, word, word_test);
          }

        }
      }
    }
    addrReg = addr + 4;
  }
  printf("[hostlink::LoadAll] written data memory\n");

  printf("[hostlink::LoadAll] checking data mem.\n");
  MemFileReader data_verify(dataFilename);
  addrReg = 0xffffffff;
  long int correct_count = 0;
  long int total_count = 0;

  while (data_verify.getWord(&addr, &word)) {
    for (int x = 0; x < meshXLen; x++) {
      for (int y = 0; y < meshYLen; y++) {
        // for (int i = 0; i < TinselDRAMsPerBoard; i++) {
        for (int core =0; core < TinselCoresPerBoard; core++) {
          // Use one core to initialise each DRAM
          //uint32_t dest = toAddr(x, y, coresPerDRAM * i, 0);
          uint32_t dest = toAddr(x, y, core, 0);
          total_count++;

          req.cmd = SetAddrCmd;
          req.numArgs = 1;
          req.args[0] = addr;
          send(dest, 1, &req, true);
          printf(".");
          fflush(stdout);

          req.cmd = LoadCmd;
          req.numArgs = 1;
          req.args[0] = 4;
          send(dest, 1, &req, true);

          bool got = 0;
          for (int rpt=0; rpt<100; rpt++) {
            if (!canRecv()) {
              got = 1;
              break;
            }
            usleep(1000);
          }
          printf("\b \b");
          fflush(stdout);

          if (!got) {
            printf("[hostlink::LoadAll] failed to get reply for datamem read req addr %i core %i:%i:%i\n", addr, x, y, core);
            continue;
          }

          recv(load_reply_buf);
          if (load_reply_buf[0] != word) {
             printf("[hostlink::LoadAll] data mismatch at addr %i core %i:%i:%i: expected %04X, got %04X\n", addr, x, y, core, word, load_reply_buf[0]);
          } else {
            correct_count++;
          }
          addrReg = addr + 4;
        }
      }
    }
  }
  printf("[hostlink::LoadAll] datamem check completed with %li words correct out of %li over all cores.\n", correct_count, total_count);
}


// Load application code and data onto the mesh, and start the cores
void HostLink::boot(const char* codeFilename, const char* dataFilename)
{
    loadAll(codeFilename, dataFilename);
    startAll();
}

// Trigger to start application execution
void HostLink::go()
{
  for (int x = 0; x < meshXLen; x++) {
    for (int y = 0; y < meshYLen; y++) {
      // for (int core=0; core<TinselCoresPerBoard; core++) {
      //   debugLink->setDest(x, y, core, 0);
      //   debugLink->put(x, y, 0);
      // }
      debugLink->setBroadcastDest(x, y, 0);
      debugLink->put(x, y, 0);
    }
  }
  printf("[HostLink::go] sent go stdin msg to all cores.\n");
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
      send(dest, 1, &req);
    }
    req.cmd = WriteInstrCmd;
    req.numArgs = 1;
    req.args[0] = word;
    send(dest, 1, &req);
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
      send(dest, 1, &req);
    }
    req.cmd = StoreCmd;
    req.numArgs = 1;
    req.args[0] = word;
    send(dest, 1, &req);
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
  printf("[HostLink::startOne] starting addr %i\n", dest);

  // Send start command
  req.cmd = StartCmd;
  req.args[0] = numThreads-1;
  send(dest, 1, &req);

  // Wait for start response
  uint32_t msg[1 << TinselLogWordsPerMsg];
  recv(msg);
  printf("[HostLink::startOne] Core %i started\n", msg[0]);
}

// Start given number of threads on given core
void HostLink::printStack(uint32_t meshX, uint32_t meshY,
       uint32_t coreId)
{
  BootReq req;
  uint32_t dest = toAddr(meshX, meshY, coreId, 0);

  // Send start command
  req.cmd = StackCmd;
  send(dest, 1, &req);
  // req.cmd = RemoteStackCmd;
  // req.args[0] = dest;
  // send(0, 1, &req);

  // Wait for start response
  uint32_t msg[1 << TinselLogWordsPerMsg];
  recv(msg);
  uint32_t recv_meshX, recv_meshY, recv_coreId, recv_threadId;
  fromAddr(msg[1], &recv_meshX, &recv_meshY, &recv_coreId, &recv_threadId);

  printf("[HostLink::printStack] Core %i (me=%i:%i:%i:%i) stack at %x\n", coreId, recv_meshX, recv_meshY, recv_coreId, recv_threadId, msg[0]);
}

void HostLink::printStackRawAddr(uint32_t addr)
{
  BootReq req;
  uint32_t recv_meshX, recv_meshY, recv_coreId, recv_threadId;
  uint32_t dest_meshX, dest_meshY, dest_coreId, dest_threadId;
  fromAddr(addr, &dest_meshX, &dest_meshY, &dest_coreId, &dest_threadId);

  // Send start command
  req.cmd = StackCmd;
  send(addr, 1, &req);
  // req.cmd = RemoteStackCmd;
  // req.args[0] = dest;
  // send(0, 1, &req);

  // Wait for start response
  uint32_t msg[1 << TinselLogWordsPerMsg];
  for (int rpt=0; rpt<1000; rpt++) {
    if (canRecv()) {
      recv(msg);
      fromAddr(msg[1], &recv_meshX, &recv_meshY, &recv_coreId, &recv_threadId);
      printf("[HostLink::printStackRawAddr] Core intended: %i:%i:%i:%i (addr %i) Core replied: %i:%i:%i:%i stack at %x\n",
             dest_meshX, dest_meshY, dest_coreId, dest_threadId, addr,
             recv_meshX, recv_meshY, recv_coreId, recv_threadId,
             msg[0]);
      return;
    }
    usleep(1000);
  }
  printf("[HostLink::printStackRawAddr] Core  %i:%i:%i:%i did not respond.\n", dest_meshX, dest_meshY, dest_coreId, dest_threadId);

}


// Start all threads on all cores
void HostLink::startAll()
{
  // Request to boot loader
  BootReq req;
  memset(&req, 0, sizeof(BootReq)); // Keep valgrind happy about un-init bytes.

  // Total number of cores
  const uint32_t numCores =
    (meshXLen*meshYLen) << TinselLogCoresPerBoard;

  // Send start command
  uint32_t started = 0;
  uint32_t msg[1 << TinselLogWordsPerMsg];
  for (int x = 0; x < meshXLen; x++) {
    for (int y = 0; y < meshYLen; y++) {
      for (int i = 0; i < TinselCoresPerBoard; i++) {
        uint32_t dest = toAddr(x, y, i, 0);
        printf("[HostLink::startAll] starting core %i dest %i.\n", i, dest);
        req.cmd = StartCmd;
        req.args[0] = 1; //(1<<TinselLogThreadsPerCore)-1;
        while (1) {
          bool ok = send(dest, 1, &req);
          if (canRecv()) {
            recv(msg);
            started++;
          }
          if (ok) break;
        }
      }
    }
  }
  printf("[HostLink::startAll] sent all start messages.\n");
  //
  std::set<uint32_t> started_cores;
  bool missing = true;
  // Wait for all start responses
  do {
    if (canRecv()) {
      recv(msg);
      printf("[HostLink::startAll] core %i waiting on go cmd.\n", msg[0]);
      started_cores.insert(msg[0]);
      started++;
    }
    usleep(10000);
    missing=false;
    for (int coreid=0; coreid<numCores*16; coreid=coreid+16) {
      if (!started_cores.count(coreid)) missing = true;
    }
  } while (missing);

  // while (started < numCores) {
  //   if (canRecv()) {
  //     recv(msg);
  //     started++;
  //   }
  // }
  printf("[HostLink::startAll] all %i cores waiting on go cmd.\n", started);
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
  memset(&req, 0, sizeof(BootReq)); // Keep valgrind happy about un-init bytes.

  req.cmd = SetAddrCmd;
  req.numArgs = 1;
  req.args[0] = addr;
  send(toAddr(meshX, meshY, coreId, 0), 1, &req);
}

// Store words to remote memory on a given board via given core
void HostLink::store(uint32_t meshX, uint32_t meshY,
                     uint32_t coreId, uint32_t numWords, uint32_t* data)
{
  BootReq req;
  memset(&req, 0, sizeof(BootReq)); // Keep valgrind happy about un-init bytes.

  req.cmd = StoreCmd;
  while (numWords > 0) {
    uint32_t sendWords = numWords > 15 ? 15 : numWords;
    numWords = numWords - sendWords;
    req.numArgs = sendWords;
    for (uint32_t i = 0; i < sendWords; i++) req.args[i] = data[i];
    uint32_t numFlits = 1 + (sendWords >> 2);
    send(toAddr(meshX, meshY, coreId, 0), numFlits, &req);
  }
}

// Power-on self test
bool HostLink::powerOnSelfTest()
{
  const double timeout = 300.0;

  // Need to check that we get a response within a given time
  struct timeval start, finish, diff;

  // Boot request to load data from memory
  // (The test involves reading from QDRII+ SRAMs on each board)
  BootReq req;
  memset(&req, 0, sizeof(BootReq)); // Keep valgrind happy about un-init bytes.

  req.cmd = LoadCmd;
  req.numArgs = 1;
  req.args[0] = 1;

  // Flit buffer to store responses
  uint32_t msg[1 << TinselLogWordsPerMsg];

  // Count number of responses received
  int count = 0;
  int sent = 0;

  // Send request and consume responses
  for (int core = 0; core < TinselCoresPerBoard; core++) {
    // int core = slice << (TinselLogCoresPerBoard-1);
    // for (int ram = 1; ram <= TinselDRAMsPerBoard; ram++) {
    for (int y = 0; y < meshYLen; y++) {
      for (int x = 0; x < meshXLen; x++) {
        sent++;
        // Request a word from SRAM
        uint32_t addr = 10; // ram << TinselLogBytesPerSRAM;
        setAddr(x, y, core, addr);
        gettimeofday(&start, NULL);
        while (1) {
          uint32_t mailbox_addr = toAddr(x, y, core, 0);
          printf("[HostLink::powerOnSelfTest] self-testing core %i:%i:%i addr 0x%04X\n", x, y, core, mailbox_addr);
          bool ok = trySend(mailbox_addr, 1, &req);
          // flushcore(mailbox_addr);
          // for (int retry=0; retry<1000; retry++) {
          //   usleep(50);
          //   if (canRecv()) {
          //     recv(msg);
          //     count++;
          //     printf("[HostLink::powerOnSelfTest] count=%i from core %i:%i:%i \n", count, x, y, core);
          //     if (ok) break;
          //   }
          // }
          if (ok) break;
          gettimeofday(&finish, NULL);
          timersub(&finish, &start, &diff);
          double duration = (double) diff.tv_sec +
                            (double) diff.tv_usec / 1000000.0;
          if (duration > timeout) return false;
        }
      }
      // }
    }
  }
  printf("[HostLink::powerOnSelfTest] sent all self-test requests count=%i\n", count);

  // Consume remaining responses
  gettimeofday(&start, NULL);
  while (count < sent) {
    usleep(500);
    if (canRecv()) {
      recv(msg);
      printf("[HostLink::powerOnSelfTest] count=%i\n", count);
      count++;
      gettimeofday(&start, NULL);
    }
    gettimeofday(&finish, NULL);
    timersub(&finish, &start, &diff);
    double duration = (double) diff.tv_sec +
                      (double) diff.tv_usec / 1000000.0;
    if (duration > timeout) return false;
  }
  printf("[HostLink::powerOnSelfTest] self-test passed.\n");
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
  uint32_t msg[1 << TinselLogWordsPerMsg];
  for (;;) {
    bool ok = pollStdOut(outFile);
    if (!ok){
      fflush(outFile); // Try to ensure output becomes visible to sink process
      usleep(10000);
    }

    if (canRecv()) {
      recv(msg);
      printf("[Hostlink flit] %x %x %x %x\n", msg[0], msg[1], msg[2], msg[3]);
    }

  }
}

// Receive lines from StdOut byte streams and append to file (blocking)
void HostLink::dumpStdOut(FILE* outFile, uint32_t lines)
{
  uint32_t count = 0;
  while (count < lines) {
    bool ok = pollStdOut(outFile, &count);
    if (!ok){
      fflush(outFile); // Try to ensure output becomes visible to sink process
      usleep(10000);
    }
  }
}

// Display UART StdOut (blocking function, never terminates)
void HostLink::dumpStdOut()
{
  dumpStdOut(stdout);
}
