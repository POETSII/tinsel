#include "Ring.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <HostLink.h>
#include <POLite.h>

int main()
{
  // Parameters
  const int numDevices    = 1024;
  const int numTokens     = 1024;
  const int numIterations = 100;

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<DefaultPThread<RingDevice> > graph;

  // Create ring of devices
  PDeviceId ring[numDevices];
  for (uint32_t i = 0; i < numDevices; i++)
    ring[i] = graph.newDevice();

  // Add edges
  for (uint32_t i = 0; i < numDevices; i++)
    graph.addEdge(ring[i], 0, ring[(i+1)%numDevices]);

  // Prepare mapping from graph to hardware
  graph.map();

  // Specify initial state of root device
  graph.devices[ring[0]]->state.root = 1;
  graph.devices[ring[0]]->state.received = numTokens;
  graph.devices[ring[0]]->state.stopCount = numTokens * (numIterations+1);
 
  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("build/code.v", "build/data.v");

  // Get start time
  printf("Starting\n");
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  // Trigger execution
  hostLink.go();

  // Wait for response
  uint32_t resp[4];
  hostLink.recv(resp);
  printf("Done\n");

  // Get finish time
  gettimeofday(&finish, NULL);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
