// SPDX-License-Identifier: BSD-2-Clause
#include "ClockTree.h"

#include <HostLink.h>
#include <POLite.h>
#include <sys/time.h>
#include <assert.h>

// Size of a tree of depth d and branching factor b
int size(int d, int b)
{
  if (d == 0)
    return 1;
  else {
    return 1 + b * size(d-1, b);
  }
}

int main(int argc, char** argv)
{
  if (argc != 3) {
    fprintf(stderr, "Expected arguments: <depth> <fanout>\n");
    return -1;
  }
  int d = atoi(argv[1]);  // Depth
  int b = atoi(argv[2]);  // Branching factor (fanout)
  assert (d > 0 && b > 0);

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<ClockTreeDevice, ClockTreeState, None, ClockTreeMessage> graph;
  graph.mapVerticesToDRAM = true;
  graph.mapEdgesToDRAM = true;

  // Number of devices in tree
  int numDevices = size(d, b);
  int numDevicesInterior = size(d-1, b);
  printf("Devices: %d\n", numDevices);

  // Create tree of devices
  PDeviceId* tree = new PDeviceId [numDevices];
  for (uint32_t i = 0; i < numDevices; i++)
    tree[i] = graph.newDevice();

  // Add edges
  for (int i = 0; i < numDevices; i++) {
    if (i < numDevicesInterior) {
      for (int j = 1; j <= b; j++) {
        graph.addEdge(tree[i], PIN_TICK, tree[b*i+j]);
        graph.addEdge(tree[b*i+j], PIN_ACK, tree[i]);
      }
    }
  }

  // Prepare mapping from graph to hardware
  graph.map();

  // Initialise devices
  graph.devices[tree[0]]->state.isRoot = true;
  for (int i = 0; i < numDevices; i++) {
    graph.devices[tree[i]]->state.ackCount = b;
    if (i >= numDevicesInterior) {
      graph.devices[tree[i]]->state.isLeaf = true;
    }
  }

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();
  printf("Starting\n");

  // Start timer
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  // Receive message
  PMessage<None, ClockTreeMessage> msg;
  hostLink.recvMsg(&msg, sizeof(msg));
  gettimeofday(&finish, NULL);

  // Display leaf count
  printf("Vertex count = %u\n", msg.payload.vertexCount);

  // Display cycle count
  printf("Cycles = %u\n", msg.payload.cycleCount);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  #ifndef POLITE_DUMP_STATS
  printf("Time = %lf\n", duration);
  #endif

  return 0;
}
