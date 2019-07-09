// SPDX-License-Identifier: BSD-2-Clause
#include "ping.h"

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS

#include <HostLink.h>
#include <POLite.h>
#include <EdgeList.h>
#include <assert.h>
#include <sys/time.h>
#include <config.h>
#include <map>

struct unit_t {
    uint16_t x;
    uint16_t y;
    uint16_t z;

    // #ifndef TINSEL // below is only needed for the host code

    // so that we can use the co-ordinate of the spatial unit as a key
    bool operator<(const unit_t& coord) const {
        if(x < coord.x) return true;
        if(x > coord.x) return false;
        //x == coord.x
        if(y < coord.y) return true;
        if(y > coord.y) return false;
        //x == coord.x && y == coord.y
        if(z < coord.z) return true;
        if(z > coord.z) return false;
        //*this == coord
        return false;
    }

    // #endif /* TINSEL */

}; // 6 bytes

void addNeighbour(PDeviceId a, PDeviceId b, PGraph<PingDevice, PingState, None, PingMessage>* graph) {
    graph->addEdge(a,0,b);
}

int main(int argc, char**argv)
{
  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<PingDevice, PingState, None, PingMessage>* graph = new PGraph<PingDevice, PingState, None, PingMessage>();

  uint32_t size = 18;
  std::map<PDeviceId, unit_t> idToLoc;
  std::map<unit_t, PDeviceId> locToId;
  // create the devices
  for(uint16_t x=0; x<size; x++) {
      for(uint16_t y=0; y<size; y++) {
          for(uint16_t z=0; z<size; z++) {
                  PDeviceId id = graph->newDevice();
                  unit_t loc = {x, y, z};
                  idToLoc[id] = loc;
                  locToId[loc] = id;
          }
      }
  }

  // connect all the devices together appropriately
  // a toroidal space (cube with periodic boundaries)
  for(uint16_t x=0; x<size; x++) {
      for(uint16_t y=0; y<size; y++) {
          for(uint16_t z=0; z<size; z++) {
              // this device
              unit_t c_loc = {x,y,z};
              PDeviceId cId = locToId[c_loc];

              // calculate the neighbour positions
              // (taking into account the periodic boundary)
              uint16_t x_neg, y_neg, z_neg;
              uint16_t x_pos, y_pos, z_pos;

              // assign the x offsets
              if(x==0) {
                  x_neg = size-1;
                  x_pos = x+1;
              } else if (x == (size-1)) {
                  x_neg = x-1;
                  x_pos = 0;
              } else {
                  x_neg = x-1;
                  x_pos = x+1;
              }

              // assign the y offsets
              if(y==0) {
                  y_neg = size-1;
                  y_pos = y+1;
              } else if (y == (size-1)) {
                  y_neg = y-1;
                  y_pos = 0;
              } else {
                  y_neg = y-1;
                  y_pos = y+1;
              }

              // assign the z offsets
              if(z==0) {
                  z_neg = size-1;
                  z_pos = z+1;
              } else if (z == (size-1)) {
                  z_neg = z-1;
                  z_pos = 0;
              } else {
                  z_neg = z-1;
                  z_pos = z+1;
              }

              unit_t n_loc;
              PDeviceId nId;
              // z = -1
              // { -1,-1,-1 },  { -1,0,-1 },  { -1, +1,-1 }
              n_loc.x = x_neg; n_loc.y = y_neg; n_loc.z = z_neg;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_neg; n_loc.y = y; n_loc.z = z_neg;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_neg; n_loc.y = y_pos; n_loc.z = z_neg;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              // { 0,-1, -1 },  { 0, 0,-1 },  { 0, +1, -1 }
              n_loc.x = x; n_loc.y = y_neg; n_loc.z = z_neg;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x; n_loc.y = y; n_loc.z = z_neg;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x; n_loc.y = y_pos; n_loc.z = z_neg;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              // { +1,-1,-1 },  { +1,0,-1 },  { +1, +1,-1 }
              n_loc.x = x_pos; n_loc.y = y_neg; n_loc.z = z_neg;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_pos; n_loc.y = y; n_loc.z = z_neg;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_pos; n_loc.y = y_pos; n_loc.z = z_neg;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              // z = 0
              // { -1,-1,0 },  { -1,0,0 },  { -1, +1,0 }
              n_loc.x = x_neg; n_loc.y = y_neg; n_loc.z = z;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_neg; n_loc.y = y; n_loc.z = z;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_neg; n_loc.y = y_pos; n_loc.z = z;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              // { 0,-1, 0 },  { 0, 0, 0 },  { 0, +1, 0 }
              n_loc.x = x; n_loc.y = y_neg; n_loc.z = z;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              // skipping! one is not a neighbour of oneself
              //n_loc.x = x; n_loc.y = y; n_loc.z = z;

              n_loc.x = x; n_loc.y = y_pos; n_loc.z = z;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              // { +1,-1, 0 },  { +1,0, 0 },  { +1, +1, 0 }
              n_loc.x = x_pos; n_loc.y = y_neg; n_loc.z = z;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_pos; n_loc.y = y; n_loc.z = z;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_pos; n_loc.y = y_pos; n_loc.z = z;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              // z = +1
              // { -1,-1,+1 },  { -1,0,+1},  { -1, +1,+1 }
              n_loc.x = x_neg; n_loc.y = y_neg; n_loc.z = z_pos;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_neg; n_loc.y = y; n_loc.z = z_pos;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_neg; n_loc.y = y_pos; n_loc.z = z_pos;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              // { 0,-1, +1 },  { 0, 0, +1 },  { 0, +1, +1 }
              n_loc.x = x; n_loc.y = y_neg; n_loc.z = z_pos;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x; n_loc.y = y; n_loc.z = z_pos;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x; n_loc.y = y_pos; n_loc.z = z_pos;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              // { +1,-1, +1 },  { +1,0, +1 },  { +1, +1, +1 }
              n_loc.x = x_pos; n_loc.y = y_neg; n_loc.z = z_pos;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_pos; n_loc.y = y; n_loc.z = z_pos;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);

              n_loc.x = x_pos; n_loc.y = y_pos; n_loc.z = z_pos;
              nId = locToId[n_loc];
              addNeighbour(cId, nId, graph);
          }
      }
  }
  // all the edges have been connected

  graph->mapVerticesToDRAM = true;
  graph->map(); // map the graph into hardware calling the POLite placer

  for(std::map<PDeviceId, unit_t>::iterator i = idToLoc.begin(); i!=idToLoc.end(); ++i) {
    PDeviceId cId = i->first;
    PDeviceAddr srcAddr = graph->toDeviceAddr[cId];
    PThreadId srcThread = getThreadId(srcAddr);
    uint32_t intraThread = 0;
    for (int j = 0; j < graph->graph.outgoing->elems[cId]->numElems; j++) {
      PDeviceId destId = graph->graph.outgoing->elems[cId]->elems[j];
      PDeviceAddr destAddr = graph->toDeviceAddr[destId];
      PThreadId destThread = getThreadId(destAddr);
      if (srcThread == destThread) {
          intraThread++;
      }
    }
    graph->devices[cId]->state.intraThreadNeighbours = intraThread;
  }

  // Write graph down to tinsel machine via HostLink
  graph->write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();

  printf("Grid started with size %u x %u x %u\n", size, size, size);

  uint32_t devsRecv = size*size*size;
  uint32_t intraThreadSent = 0;
  uint32_t intraThreadRecv = 0;
  uint32_t interThreadSent = 0;
  uint32_t interThreadRecv = 0;

  while (1) {
    PMessage<None, PingMessage> msg;
    hostLink.recvMsg(&msg, sizeof(msg));

    intraThreadSent += msg.payload.intraThreadSent;
    intraThreadRecv += msg.payload.intraThreadRecv;
    interThreadSent += msg.payload.interThreadSent;
    interThreadRecv += msg.payload.interThreadRecv;

    devsRecv--;
    printf("devsRecv = %u\n", devsRecv);
    if (devsRecv == 0) {
      break;
    }

  }
  printf("Finished\n");
  // Consume performance stats
  politeSaveStats(&hostLink, "stats.txt");

  printf("intraThreadSent = %u\n", intraThreadSent);
  printf("intraThreadRecv = %u\n", intraThreadRecv);
  printf("interThreadSent = %u\n", interThreadSent);
  printf("interThreadRecv = %u\n", interThreadRecv);

  return 0;
}
