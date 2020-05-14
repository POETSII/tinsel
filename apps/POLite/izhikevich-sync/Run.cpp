// SPDX-License-Identifier: BSD-2-Clause
#include "Izhikevich.h"

#include <HostLink.h>
#include <POLite.h>

#include <EdgeList.h>
#include <assert.h>
#include <sys/time.h>
#include <config.h>

inline double urand() { return (double) rand() / RAND_MAX; }

int main(int argc, char**argv)
{
  if (argc != 2) {
    printf("Specify edges file\n");
    exit(EXIT_FAILURE);
  }

  // Read network
  EdgeList net;
  net.read(argv[1]);
  printf("Max fan-out = %d\n", net.maxFanOut());
  printf("Min fan-out = %d\n", net.minFanOut());

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<IzhikevichDevice, IzhikevichState, Weight, IzhikevichMsg> graph;

  // Create nodes in POETS graph
  for (uint32_t i = 0; i < net.numNodes; i++) {
    PDeviceId id = graph.newDevice();
    assert(i == id);
  }

  // Ratio of excitatory to inhibitory neurons
  double excitatory = 0.8;

  // Mark each neuron as excitatory (or inhibiatory)
  srand(1);
  bool* excite = new bool [net.numNodes];
  for (int i = 0; i < net.numNodes; i++)
    excite[i] = urand() < excitatory;

  // Create connections in POETS graph
  for (uint32_t i = 0; i < net.numNodes; i++) {
    uint32_t numNeighbours = net.neighbours[i][0];
    for (uint32_t j = 0; j < numNeighbours; j++) {
      float weight = excite[i] ? 0.5 * urand() : -urand();
      graph.addLabelledEdge(weight, i, 0, net.neighbours[i][j+1]);
    }
  }

  // Prepare mapping from graph to hardware
  graph.map();

  srand(2);
  // Initialise devices
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    IzhikevichState* n = &graph.devices[i]->state;
    n->rng = (int32_t) (urand()*((double) (1<<31)));
    if (excite[i]) {
      float re = (float) urand();
      n->a = 0.02;
      n->b = 0.2;
      n->c = -65+15*re*re;
      n->d = 8-6*re*re;
      n->Ir = 5;
    }
    else {
      float ri = (float) urand();
      n->a = 0.02+0.08*ri;
      n->b = 0.25-0.05*ri;
      n->c = -65;
      n->d = 2;
      n->Ir = 2;
    }
  }

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();

  // Timer
  printf("Started\n");
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  // Consume performance stats
  politeSaveStats(&hostLink, "stats.txt");

  int64_t sum = 0;
  // Receive final distance to each vertex
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    // Receive message
    PMessage<IzhikevichMsg> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    if (i == 0) gettimeofday(&finish, NULL);
    // Accumulate
    sum += msg.payload.spikeCount;
  }

  // Emit result
  printf("Total spikes = %ld\n", sum);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
