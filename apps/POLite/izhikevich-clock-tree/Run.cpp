// SPDX-License-Identifier: BSD-2-Clause
#include "Izhikevich.h"

#include <HostLink.h>
#include <POLite.h>

#include <EdgeList.h>
#include <assert.h>
#include <sys/time.h>
#include <config.h>

inline double urand() { return (double) rand() / RAND_MAX; }

using IzhikevichGraph = PGraph<IzhikevichDevice, IzhikevichState, Weight, IzhikevichMsg>;

void build_clock_tree(IzhikevichGraph &graph, unsigned parent, unsigned begin, unsigned end)
{
  assert(begin<=end);
  if(begin==end){
    // Do nothing
  }else if(begin+1==end){
    graph.addLabelledEdge(0.0f, parent, TICK_OUT, begin);
    graph.addLabelledEdge(0.0f, begin, TOCK_OUT, parent);
    graph.devices[parent]->state.clock_child_count=1;
  }else{
    unsigned mid=(begin+end)/2;

    graph.addLabelledEdge(0.0f, parent, TICK_OUT, begin);
    graph.addLabelledEdge(0.0f, begin, TOCK_OUT, parent);
    build_clock_tree(graph, begin, begin+1, mid);

    graph.addLabelledEdge(0.0f, parent, TICK_OUT, mid);
    graph.addLabelledEdge(0.0f, mid, TOCK_OUT, parent);
    build_clock_tree(graph, mid, mid+1, end);

    graph.devices[parent]->state.clock_child_count=2;
  }
}

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
  IzhikevichGraph graph;

  unsigned num_steps=1024;

  // Create nodes in POETS graph
  for (uint32_t i = 0; i < net.numNodes; i++) {
    PDeviceId id = graph.newDevice();
    assert(i == id);
    graph.devices[i]->state.num_steps=num_steps;
  }

  build_clock_tree(graph, 0, 1, net.numNodes);
  graph.devices[0]->state.is_root=1;


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
      graph.addLabelledEdge(weight, i, SPIKE_OUT, net.neighbours[i][j+1]);
    }
  }

  // Prepare mapping from graph to hardware
  graph.map();

  srand(2);
  // Initialise devices
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    IzhikevichState* n = &graph.devices[i]->state;
    n->id=i;
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

  std::vector<SpikeStatsCollector> collected_stats(graph.numDevices);
  unsigned complete_device_stats=0;
  while(complete_device_stats < collected_stats.size()){  
    PMessage<IzhikevichMsg> msg;
    hostLink.recvMsg(&msg, sizeof(msg));

    unsigned id=msg.payload.src;
    auto &stats=collected_stats.at(id);
    assert(!stats.complete());

    if(!stats.import_msg(msg.payload.bytes)){
      complete_device_stats++;
    }
  }

  gettimeofday(&finish, NULL);


  int min_spike_delta=INT32_MAX;
  int max_spike_delta=INT32_MIN;
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    collected_stats[i].msg.dump(i);
    min_spike_delta=std::min<int>(min_spike_delta, collected_stats[i].msg.recv_delta_min);
    max_spike_delta=std::max<int>(max_spike_delta, collected_stats[i].msg.recv_delta_max);
  }
  fprintf(stderr, "Delta in [%d,%d]\n", min_spike_delta, max_spike_delta);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  #ifndef POLITE_DUMP_STATS
  printf("Time = %lf\n", duration);
  #endif

  return 0;
}
