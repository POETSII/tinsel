// SPDX-License-Identifier: BSD-2-Clause
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>

#include <config.h>
#include <DebugLink.h>
#include <SocketUtils.h>

// Names of boxes in box mesh
static const char* boxMesh[][TinselBoxMeshXLen] =
  TinselBoxMesh;

// Helper: blocking receive of a BoardCtrlPkt
void DebugLink::getPacket(int x, int y, BoardCtrlPkt* pkt)
{
  int got = 0;
  char* buf = (char*) pkt;
  int numBytes = sizeof(BoardCtrlPkt);
  while (numBytes > 0) {
    int ret = recv(conn[y][x], &buf[got], numBytes, 0);
    if (ret < 0) {
      fprintf(stderr, "Connection to box '%s' failed ",
        boxMesh[thisBoxY+y][thisBoxX+x]);
      fprintf(stderr, "(box may already be in use)\n");
      exit(EXIT_FAILURE);
    }
    else {
      got += ret;
      numBytes -= ret;
    }
  }
}

// Helper: blocking send of a BoardCtrlPkt
void DebugLink::putPacket(int x, int y, BoardCtrlPkt* pkt)
{
  int sent = 0;
  char* buf = (char*) pkt;
  int numBytes = sizeof(BoardCtrlPkt);
  while (numBytes > 0) {
    int ret = send(conn[y][x], &buf[sent], numBytes, 0);
    if (ret < 0) {
      fprintf(stderr, "Connection to box '%s' failed ",
        boxMesh[thisBoxY+y][thisBoxX+x]);
      fprintf(stderr, "(box may already be in use)\n");
      exit(EXIT_FAILURE);
    }
    else {
      sent += ret;
      numBytes -= ret;
    }
  }
}

// Constructor
DebugLink::DebugLink(uint32_t numBoxesX, uint32_t numBoxesY)
{
  boxMeshXLen = numBoxesX;
  boxMeshYLen = numBoxesY;
  get_tryNextX = 0;
  get_tryNextY = 0;

  // Get the name of the box we're running on
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)-1)) {
    perror("gethostname()");
    exit(EXIT_FAILURE);
  }
  hostname[sizeof(hostname)-1] = '\0';

  // Preprocess hostname (make lower case, drop domain name)
  for (unsigned i = 0; i < strlen(hostname); i++) {
    if (hostname[i] == '.') {
      hostname[i] = '\0';
      break;
    }
    hostname[i] = tolower(hostname[i]);
  }

  // Ensure the requested submesh size is valid from this box
  thisBoxX = -1;
  thisBoxY = -1;
  for (int j = 0; j < TinselBoxMeshYLen; j++) {
    for (int i = 0; i < TinselBoxMeshXLen; i++) {
      if (strcmp(boxMesh[j][i], hostname) == 0) {
        thisBoxX = i;
        thisBoxY = j;
      }
    }
  }
  if (thisBoxX == -1 || thisBoxY == -1) {
    fprintf(stderr, "Box '%s' not recognised as a POETS box\n", hostname);
    exit(EXIT_FAILURE);
  }
  if (thisBoxX > 0) {
    fprintf(stderr, "This machine (the origin of the box sub-mesh) "
                    "must have a box X coordinate of 0\n"
                    "But is has a box X coordinate of %i\n", thisBoxX);
    exit(EXIT_FAILURE);
  }
  if ((thisBoxX+numBoxesX-1) >= TinselBoxMeshXLen ||
      (thisBoxY+numBoxesY-1) >= TinselBoxMeshYLen) {
    fprintf(stderr, "Requested box sub-mesh of size %ix%i "
                    "is not valid from box %s\n",
                    numBoxesX, numBoxesY, hostname);
    exit(EXIT_FAILURE);
  }

  // Dimensions of the global board mesh
  meshXLen = TinselMeshXLenWithinBox * boxMeshXLen;
  meshYLen = TinselMeshYLenWithinBox * boxMeshYLen;

  // Allocate member structures
  conn = new int* [boxMeshYLen];
  for (int y = 0; y < boxMeshYLen; y++)
    conn[y] = new int [boxMeshXLen];

  bridge = new int* [boxMeshYLen];
  for (int y = 0; y < boxMeshYLen; y++) {
    bridge[y] = new int [boxMeshXLen];
    for (int x = 0; x < meshXLen; x++)
      bridge[y][x] = -1;
  }

  boxX = new int* [meshYLen];
  for (int y = 0; y < meshYLen; y++) {
    boxX[y] = new int [meshXLen];
    for (int x = 0; x < meshXLen; x++)
      boxX[y][x] = -1;
  }

  boxY = new int* [meshYLen];
  for (int y = 0; y < meshYLen; y++)
    boxY[y] = new int [meshXLen];

  linkId = new int* [meshYLen];
  for (int y = 0; y < meshYLen; y++)
    linkId[y] = new int [meshXLen];

  boardX = new int** [boxMeshYLen];
  for (int y = 0; y < boxMeshYLen; y++) {
    boardX[y] = new int* [boxMeshXLen];
    for (int x = 0; x < boxMeshXLen; x++)
      boardX[y][x] = new int [TinselBoardsPerBox];
  }

  boardY = new int** [boxMeshYLen];
  for (int y = 0; y < boxMeshYLen; y++) {
    boardY[y] = new int* [boxMeshXLen];
    for (int x = 0; x < boxMeshXLen; x++)
      boardY[y][x] = new int [TinselBoardsPerBox];
  }

  // Connect to boardctrld on each box
  for (int y = 0; y < boxMeshYLen; y++)
    for (int x = 0; x < boxMeshXLen; x++)
      conn[y][x] = socketConnectTCP(boxMesh[thisBoxY+y][thisBoxX+x], 10101);

  // Receive ready packets from each box
  BoardCtrlPkt pkt;
  for (int y = 0; y < boxMeshYLen; y++)
    for (int x = 0; x < boxMeshXLen; x++) {
      getPacket(x, y, &pkt);
      assert(pkt.payload[0] == DEBUGLINK_READY);
    }

  // Send queries
  pkt.payload[0] = DEBUGLINK_QUERY_IN;
  for (int y = 0; y < boxMeshYLen; y++)
    for (int x = 0; x < boxMeshXLen; x++) {
      // Determine offset for each board in box
      int offsetX = x * TinselMeshXLenWithinBox;
      int offsetY = y * TinselMeshYLenWithinBox;
      assert(offsetX < 16);
      assert(offsetY < 16);
      pkt.payload[1] = (offsetY << 4) | offsetX;
      // Sandbox this application from others running on the cluster
      pkt.payload[2] = 0;
      if (y == boxMeshYLen-1) pkt.payload[2] |= 1;
      if (y == 0) pkt.payload[2] |= 2;
      if (thisBoxX == 0 && boxMeshXLen == 1) pkt.payload[2] |= 4;
      if (thisBoxX == 1 && boxMeshXLen == 1) pkt.payload[2] |= 8;
      // Send commands to each board
      for (int b = 0; b < TinselBoardsPerBox; b++) {
        pkt.linkId = b;
        putPacket(x, y, &pkt);
      }
    }

  // Receive query responses
  for (int y = 0; y < boxMeshYLen; y++)
    for (int x = 0; x < boxMeshXLen; x++) {
      for (int b = 0; b < TinselBoardsPerBox; b++) {
        getPacket(x, y, &pkt);
        assert(pkt.payload[0] == DEBUGLINK_QUERY_OUT);
        if (pkt.payload[1] == 0) {
          if (bridge[y][x] != -1) {
            fprintf(stderr, "Too many bridge boards detected on box %s\n",
              boxMesh[thisBoxX+y][thisBoxY+x]);
          }
          // It's a bridge board, let's remember its link id
          bridge[y][x] = pkt.linkId;
        }
        else {
          // It's a worker board, let's work out its mesh coordinates
          int id = pkt.payload[1] - 1;
          int subX = id & ((1 << TinselMeshXBitsWithinBox) - 1);
          int subY = id >> TinselMeshXBitsWithinBox;
          assert(subX < TinselMeshXLenWithinBox);
          assert(subY < TinselMeshYLenWithinBox);
          // Full X and Y coordinates on the global board mesh
          int fullX = x*TinselMeshXLenWithinBox + subX;
          int fullY = y*TinselMeshYLenWithinBox + subY;
          assert(boxX[fullY][fullX] == -1);
          // Populate bidirectional mappings
          boardX[y][x][pkt.linkId] = fullX;
          boardY[y][x][pkt.linkId] = fullY;
          boxX[fullY][fullX] = x;
          boxY[fullY][fullX] = y;
          linkId[fullY][fullX] = pkt.linkId;
        }
      }
  }

  // Query the bridge board on the master box a second time to
  // enable idle-detection (only now do all the boards know their
  // full coordinates in the mesh).
  pkt.payload[0] = DEBUGLINK_EN_IDLE;
  pkt.payload[1] = (meshYLen << 4) | meshXLen;
  pkt.linkId = bridge[0][0];
  putPacket(0, 0, &pkt);
  // Get response
  getPacket(0, 0, &pkt);
  assert(pkt.payload[0] == DEBUGLINK_QUERY_OUT);
}

// On given board, set destination core and thread
void DebugLink::setDest(uint32_t boardX, uint32_t boardY,
                  uint32_t coreId, uint32_t threadId)
{
  BoardCtrlPkt pkt;
  pkt.linkId = linkId[boardY][boardX];
  // SetDest command
  pkt.payload[0] = DEBUGLINK_SET_DEST;
  // Core-local thread id
  pkt.payload[1] = threadId;
  // Board-local core id
  pkt.payload[2] = coreId;
  // Send packet to appropriate box
  putPacket(boxX[boardY][boardX], boxY[boardY][boardX], &pkt);
}

// On given board, set destinations to core-local thread id on every core
void DebugLink::setBroadcastDest(
       uint32_t boardX, uint32_t boardY, uint32_t threadId)
{
  BoardCtrlPkt pkt;
  pkt.linkId = linkId[boardY][boardX];
  // SetDest command
  pkt.payload[0] = DEBUGLINK_SET_DEST;
  // Core-local thread id
  pkt.payload[1] = threadId;
  // Broadcast address
  pkt.payload[2] = 0x80;
  // Send packet to appropriate box
  putPacket(boxX[boardY][boardX], boxY[boardY][boardX], &pkt);
}

// On given board, send byte to destination thread (StdIn)
void DebugLink::put(uint32_t boardX, uint32_t boardY, uint8_t byte)
{
  BoardCtrlPkt pkt;
  pkt.linkId = linkId[boardY][boardX];
  pkt.payload[0] = DEBUGLINK_STD_IN;
  pkt.payload[1] = byte;
  putPacket(boxX[boardY][boardX], boxY[boardY][boardX], &pkt);
}

// Receive byte (StdOut)
void DebugLink::get(uint32_t* brdX, uint32_t* brdY,
                      uint32_t* coreId, uint32_t* threadId, uint8_t* byte)
{
  BoardCtrlPkt pkt;
  int x = get_tryNextX;
  int y = get_tryNextY;
  bool done = false;
  while (!done) {
    // Consider boxes fairly between calls to get()
    if (socketCanGet(conn[y][x])) {
      getPacket(x, y, &pkt);
      if (pkt.payload[0] != DEBUGLINK_STD_OUT) {
        fprintf(stderr, "DebugLink: unexpected response (not StdOut)\n");
        exit(EXIT_FAILURE);
      }
      *brdX = boardX[y][x][pkt.linkId];
      *brdY = boardY[y][x][pkt.linkId];
      *coreId = (uint32_t) pkt.payload[2];
      *threadId = (uint32_t) pkt.payload[1];
      *byte = pkt.payload[3];
      done = true;
    }
    // Try next box
    x++;
    if (x == boxMeshXLen) {
      x = 0;
      y = (y+1) % boxMeshYLen;
    }
    get_tryNextX = x;
    get_tryNextY = y;
  }
}

// Is a data available for reading?
bool DebugLink::canGet()
{
  for (int x = 0; x < boxMeshXLen; x++)
    for (int y = 0; y < boxMeshYLen; y++)
      if (socketCanGet(conn[y][x])) return true;
  return false;
}

// Read temperature of given board
int32_t DebugLink::getTemp(uint32_t boardX, uint32_t boardY)
{
  BoardCtrlPkt pkt;
  pkt.linkId = linkId[boardY][boardX];
  pkt.payload[0] = DEBUGLINK_TEMP_IN;
  putPacket(boxX[boardY][boardX], boxY[boardY][boardX], &pkt);
  getPacket(boxX[boardY][boardX], boxY[boardY][boardX], &pkt);
  assert(pkt.payload[0] == DEBUGLINK_TEMP_OUT);
  return ((int32_t) pkt.payload[1]) - 128;
}

// Destructor
DebugLink::~DebugLink()
{
  // Close connections
  for (int y = 0; y < boxMeshYLen; y++)
    for (int x = 0; x < boxMeshXLen; x++)
      close(conn[y][x]);

  // Deallocate member structures
  for (int y = 0; y < boxMeshYLen; y++)
    delete [] conn[y];
  delete [] conn;

  for (int y = 0; y < boxMeshYLen; y++)
    delete [] bridge[y];
  delete [] bridge;

  for (int y = 0; y < meshYLen; y++)
    delete [] boxX[y];
  delete [] boxX;

  for (int y = 0; y < meshYLen; y++)
    delete [] boxY[y];
  delete [] boxY;

  for (int y = 0; y < meshYLen; y++)
    delete [] linkId[y];
  delete [] linkId;

  for (int y = 0; y < boxMeshYLen; y++) {
    for (int x = 0; x < boxMeshXLen; x++)
      delete [] boardX[y][x];
    delete [] boardX[y];
  }
  delete [] boardX;

  for (int y = 0; y < boxMeshYLen; y++) {
    for (int x = 0; x < boxMeshXLen; x++)
      delete [] boardY[y][x];
    delete [] boardY[y];
  }
  delete [] boardY;
}
