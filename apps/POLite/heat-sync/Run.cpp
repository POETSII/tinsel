// SPDX-License-Identifier: BSD-2-Clause
#include "Heat.h"
#include "Colours.h"

#include <HostLink.h>
#include <POLite.h>
#include <sys/time.h>

int main()
{
  // Parameters
  const uint32_t width  = 256;
  const uint32_t height = 256;
  const uint32_t time   = 1000;

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<HeatDevice, HeatState, None, HeatMessage> graph;

  // Create 2D mesh of devices
  PDeviceId mesh[height][width];
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++)
      mesh[y][x] = graph.newDevice();

  // Add edges
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++) {
      if (x < width-1) {
        graph.addEdge(mesh[y][x],   0, mesh[y][x+1]);
        graph.addEdge(mesh[y][x+1], 0, mesh[y][x]);
      }
      if (y < height-1) {
        graph.addEdge(mesh[y][x],   0, mesh[y+1][x]);
        graph.addEdge(mesh[y+1][x], 0, mesh[y][x]);
      }
    }

  // Prepare mapping from graph to hardware
  graph.map();

  // Set device ids
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++)
      graph.devices[mesh[y][x]]->state.id = mesh[y][x];

  // Specify number of time steps to run on each device
  for (PDeviceId i = 0; i < graph.numDevices; i++)
    graph.devices[i]->state.time = time;
 
  // Apply constant heat at north edge
  // Apply constant cool at south edge
  for (uint32_t x = 0; x < width; x++) {
    graph.devices[mesh[0][x]]->state.val = 255 << 16;
    graph.devices[mesh[0][x]]->state.isConstant = true;
    graph.devices[mesh[height-1][x]]->state.val = 40 << 16;
    graph.devices[mesh[height-1][x]]->state.isConstant = true;
  }

  // Apply constant heat at west edge
  // Apply constant cool at east edge
  for (uint32_t y = 0; y < height; y++) {
    graph.devices[mesh[y][0]]->state.val = 255 << 16;
    graph.devices[mesh[y][0]]->state.isConstant = true;
    graph.devices[mesh[y][width-1]]->state.val = 40 << 16;
    graph.devices[mesh[y][width-1]]->state.isConstant = true;
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

  // Allocate array to contain final value of each device
  uint32_t pixels[graph.numDevices];

  // Receive final value of each device
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    // Receive message
    PMessage<None, HeatMessage> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    if (i == 0) gettimeofday(&finish, NULL);
    // Save final value
    pixels[msg.payload.from] = msg.payload.val;
  }

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  // Emit image
  FILE* fp = fopen("out.ppm", "wt");
  if (fp == NULL) {
    printf("Can't open output file for writing\n");
    return -1;
  }
  fprintf(fp, "P3\n%d %d\n255\n", width, height);
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++) {
      uint32_t val = (pixels[mesh[y][x]] >> 16) & 0xff;
      fprintf(fp, "%d %d %d\n",
        colours[val*3], colours[val*3+1], colours[val*3+2]);
    }
  fclose(fp);

  return 0;
}
