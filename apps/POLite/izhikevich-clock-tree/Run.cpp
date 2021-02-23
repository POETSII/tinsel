// SPDX-License-Identifier: BSD-2-Clause
#include "Izhikevich.h"

#include <HostLink.h>
#include <POLite.h>

#include <EdgeList.h>
#include <assert.h>
#include <sys/time.h>
#include <config.h>
#include <unistd.h>

#include <thread>
#include <atomic>

inline double urand() { return (double) rand() / RAND_MAX; }

using IzhikevichGraph = PGraph<IzhikevichDevice, IzhikevichState, Weight, IzhikevichMsg>;

void build_clock_tree(IzhikevichGraph &graph, std::vector<IzhikevichState> &states, unsigned parent, unsigned begin, unsigned end)
{
  assert(begin<=end);
  if(begin==end){
    // Do nothing
  }else if(begin+1==end){
    graph.addLabelledEdge(0.0f, parent, MessageType::Tick, begin);
    graph.addLabelledEdge(0.0f, begin, MessageType::Tock, parent);
    states[parent].clock_child_count=1;
  }else{
    unsigned mid=(begin+end)/2;

    graph.addLabelledEdge(0.0f, parent, MessageType::Tick, begin);
    graph.addLabelledEdge(0.0f, begin, MessageType::Tock, parent);
    build_clock_tree(graph, states, begin, begin+1, mid);

    graph.addLabelledEdge(0.0f, parent, MessageType::Tick, mid);
    graph.addLabelledEdge(0.0f, mid, MessageType::Tock, parent);
    build_clock_tree(graph, states, mid, mid+1, end);

    states[parent].clock_child_count=2;
  }
}

int main(int argc, char**argv)
{
  if (argc != 2) {
    printf("Specify edges file\n");
    exit(EXIT_FAILURE);
  }

  // Read network
  printf("Reading edges...\n");
  EdgeList net;
  net.read(argv[1]);
  printf("Max fan-out = %d\n", net.maxFanOut());
  printf("Min fan-out = %d\n", net.minFanOut());
  fflush(stdout);

  // Connection to tinsel machine
  printf("Opening hostlink...\n");
  HostLink hostLink;

  // Create POETS graph
  IzhikevichGraph graph;
  graph.chatty=1;

  unsigned num_steps=1000;

  // Used to hold states until the graph allocates them
  std::vector<IzhikevichState> states(net.numNodes);

  // Create nodes in POETS graph
  for (uint32_t i = 0; i < net.numNodes; i++) {
    PDeviceId id = graph.newDevice();
    assert(i == id);
    states[i].num_steps=num_steps;
  }

  printf("Building clock tree...\n");
  build_clock_tree(graph, states, 0, 1, net.numNodes);
  states[0].is_root=1;


  // Ratio of excitatory to inhibitory neurons
  double excitatory = 0.8;

  // Mark each neuron as excitatory (or inhibiatory)
  srand(1);
  bool* excite = new bool [net.numNodes];
  for (int i = 0; i < net.numNodes; i++)
    excite[i] = urand() < excitatory;

  printf("Building synapse connections...\n");
  // Create connections in POETS graph
  for (uint32_t i = 0; i < net.numNodes; i++) {
    uint32_t numNeighbours = net.neighbours[i][0];
    for (uint32_t j = 0; j < numNeighbours; j++) {
      float weight = excite[i] ? 0.5 * urand() : -urand();
      graph.addLabelledEdge(weight, i, MessageType::Spike, net.neighbours[i][j+1]);
    }
  }

  // Prepare mapping from graph to hardware
  printf("Mapping  graph...\n");
  graph.map();

  printf("Initialising devices...\n");
  srand(2);
  // Initialise devices
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    IzhikevichState* n = &graph.devices[i]->state;
    *n = states[i];

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
  printf("Writing graph to hardware...\n");
  graph.write(&hostLink);

  // Load code and trigger execution
  printf("Booting...\n");
  hostLink.boot("code.v", "data.v");
  printf("Go!\n");
  hostLink.go();

  std::atomic<bool> quit_log;
  quit_log=false;

  std::thread dumper([&](){
    while(!quit_log.load()){
      bool ok=hostLink.pollStdOut(stderr);
      if(!ok){
        usleep(10000);
      }
    }
  });

  // Timer
  printf("Started\n");
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  fprintf(stderr, "Collecting spikes\n");
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

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  #ifndef POLITE_DUMP_STATS
  printf("Time = %lf\n", duration);
  #endif


  int min_spike_delta=INT32_MAX;
  int max_spike_delta=INT32_MIN;
  uint32_t max_sent=0, min_sent=UINT32_MAX;
  uint64_t total_sent=0;
  uint64_t total_recv=0;
  double sum_spike_delta=0;
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    //collected_stats[i].msg.dump(i);
    min_sent=std::min<uint32_t>(min_sent, collected_stats[i].msg.sent);
    max_sent=std::max<uint32_t>(max_sent, collected_stats[i].msg.sent);
    min_spike_delta=std::min<int>(min_spike_delta, collected_stats[i].msg.recv_delta_min);
    max_spike_delta=std::max<int>(max_spike_delta, collected_stats[i].msg.recv_delta_max);
    sum_spike_delta += collected_stats[i].msg.recv_delta_sum;
    total_sent += collected_stats[i].msg.sent;
    total_recv += collected_stats[i].msg.received;
  }
  fprintf(stderr, "Delta in [%d,%d], mean=%g\n", min_spike_delta, max_spike_delta, sum_spike_delta/total_recv);
  fprintf(stderr, "Sent in [%u,%u], average=%g\n", min_sent, max_sent, total_sent/(double)graph.numDevices);
  fprintf(stderr, "Total: sent=%llu spikes/sec=%g, spikes/neuron/sec=%g, recv=%llu\n",
    total_sent, total_sent/duration, total_sent/duration/graph.numDevices, total_recv);
  fprintf(stderr, "Steps/sec=%g, NueronSteps/sec=%g\n", num_steps/duration, num_steps*(double)graph.numDevices/duration);

  

  quit_log.store(true);
  dumper.join();

  return 0;
}
