#include <algorithm>
#include <random>
#include <chrono>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "spmm.h"

#include <HostLink.h>
#include <POLite.h>

#include <benchmark/benchmark.h>

int counter = 1;
bool toggle_flag = true;

using Nodes = std::vector<PDeviceAddr>;
using LayerSizes = std::vector<std::pair<SPMMState::Type, int>>;

void bench_tinsel(benchmark::State &st, HostLink &hostLink, const Nodes& inputAddrs, const Nodes& allNodes, RING_TYPE solution)
{
  const RING_TYPE init_value = 1;

  RING_TYPE value = 0;        // init_value;
  RING_TYPE target_value = 0; // solution;

  PMessage<None, SPMMMessage> recv_msg;
  PMessage<None, SPMMMessage> push_msg;
  push_msg.payload.update_ts = -1;

  auto seedInputs = [&inputAddrs, &hostLink](int v) {
    PMessage<None, SPMMMessage> send_msg;

    for (PDeviceAddr addr : inputAddrs){
      if(DEBUG_VERBOSITY > 1) {
        printf("HOST: Sending value %i to thread_id=0x%x dev_id=0x%x uc=0x%x\n", v, addr.threadId, addr.devId, send_msg.payload.update_ts);
      }

      send_msg.devId = addr.devId;
      send_msg.payload.value = v; // distribution(generator);
      send_msg.payload.src = 0;
      send_msg.payload.update_ts = counter++; // higher than everything but not triggering
      
      // needs to be a thread address
      hostLink.send(addr.threadId, 2, &send_msg);
    }
  };
    
  for (auto _ : st)
  {
    // switch the expected value
    if (toggle_flag)
    {
      value = init_value;
      target_value = solution;
    }
    else
    {
      value = 0;
      target_value = 0;
    }
    toggle_flag = !toggle_flag;

    // send messages to all the inputs
    seedInputs(value);

    while(true) {
      if(true or DEBUG_VERBOSITY > 1) {
        hostLink.pollStdOut();
      }
      
      bool h = hostLink.canRecv();
      if (h)
      {
        hostLink.recvMsg(&recv_msg, sizeof(PMessage<None, SPMMMessage>));
        // last_recv = now;
        // last_recv_done = false;

        if(DEBUG_VERBOSITY > 0) {

          if constexpr (std::is_same<RING_TYPE, float>::value) {
            printf("HOST: Received output dest=%i exp=%f v=%f src=0x%x "
                "last_update=%x %i\n",
                recv_msg.devId, target_value, recv_msg.payload.value, recv_msg.payload.src,
                recv_msg.payload.update_ts, recv_msg.payload.update_ts);
          } else {
            printf("HOST: Received output dest=%i exp=%i v=%i src=0x%x "
                "last_update=%x %i\n",
                recv_msg.devId, target_value, recv_msg.payload.value, recv_msg.payload.src,
                recv_msg.payload.update_ts, recv_msg.payload.update_ts);
          }
          
        }

        if (recv_msg.payload.value == target_value)
        {
          if(DEBUG_VERBOSITY > 0) {
            printf("HOST: Value is correct\n");
          }
          break;
        }
      }
      //x++;
    }
    // usleep(10000);
    // printf("finished an iteration of the bench\n");
  }

  // printf("finished iterations\n");
}

struct ThreadIdLayout
{ 
  unsigned boardY:TinselMeshYBits;            // 2
  unsigned boardX:TinselMeshXBits;            // 2
  unsigned boxY:TinselMailboxMeshYBits;       // 2
  unsigned boxX:TinselMailboxMeshXBits;       // 2
  unsigned threadNum:6;                       // 2 + 4
};

template <typename T>
void dump_graph(T& graph) {
  
  FILE * out = stderr;

  auto printLocation = [&graph, out](PDeviceId pdid) {
    PDeviceAddr pdaddr = graph.toDeviceAddr[pdid];
    PLocalDeviceId pldaddr = pdaddr.devId;
    PThreadId ptid = pdaddr.threadId;
    union C {
      ThreadIdLayout l;
      PThreadId ptid;
    } c;
    c.ptid = ptid;
    fprintf(out, "%u,%u,%u,%u,%u,%u", c.l.boardY, c.l.boardX, c.l.boxY, c.l.boxX, c.l.threadNum, pldaddr);
  };
  
  fprintf(out,"fromBoardY,fromBoardX,fromBoxY,fromBoxX,fromThreadNum,fromDeviceNum,");
  fprintf(out,"toBoardY,toBoardX,toBoxY,toBoxX,toThreadNum,toDeviceNum\n");

  for(NodeId start_node = 0; start_node < graph.graph.outgoing->numElems; start_node++) {
    Seq<NodeId>* end_nodes = graph.graph.outgoing->elems[start_node];
    
    for(NodeId end_node_idx = 0; end_node_idx < end_nodes->numElems; end_node_idx++) {
      NodeId end_node = end_nodes->elems[end_node_idx];

      printLocation(start_node);
      fprintf(out,",");
      printLocation(end_node);
      fprintf(out,"\n");
    }
  }
}

template <typename T>
void init_graph(T &graph, LayerSizes &layer_sizes, Nodes& inputs, Nodes& all_nodes, int trigger_time)
{
  // Create POETS graph
  const RING_TYPE init_weight = 2;

  struct Layer
  {
    Layer(SPMMState::Type t) : type(t) {}
    SPMMState::Type type;
    std::vector<PDeviceId> devices;
  };
  std::vector<Layer> layers;

  for (auto &layer : layer_sizes)
  {
    layers.push_back(Layer(layer.first));
    Layer &l = layers.back();

    //printf("Creating layer of size=%i\n", layer.second);
    for (int i = 0; i < layer.second; i++)
    {
      l.devices.push_back(graph.newDevice());
    }
  }

  for (int x = 1; x < layers.size(); x++)
  {
    Layer &l = layers[x];
    for (auto pdid_this : l.devices)
    {
      for (auto pdid_prev : layers[x - 1].devices)
      {
        graph.addEdge(pdid_prev, 0, pdid_this);
      }
    }
  }
  // Prepare mapping from graph to hardware
  graph.map();
  dump_graph(graph);

  for (auto &l : layers)
  {
    for (auto pdid : l.devices)
    {
      graph.devices[pdid]->state.type = l.type;
      graph.devices[pdid]->state.init_weight = init_weight;
      graph.devices[pdid]->state.triggerTime = trigger_time;
    }
  }

  for (auto pdid : layers[0].devices)
  {
    inputs.push_back(graph.toDeviceAddr[pdid]);
  }

  for (auto& l : layers)
  {
    for (auto pdid : l.devices)
    {
      all_nodes.push_back(graph.toDeviceAddr[pdid]);
    }
  }
}

int main(int argc, char **argv)
{
  std::vector<std::pair<SPMMState::Type, int>> layers;
  
  const int num_layers = std::atoi(argv[1]);
  const int layer_size = std::atoi(argv[2]);
  const int trigger_time = std::atoi(argv[3]);
  
  for(int i = 0; i < num_layers; i++) {
    layers.push_back({SPMMState::MIDDLE, layer_size});
  }
  layers.push_back({SPMMState::OUTPUT, 1});
  setlinebuf(stdout);

  int total = 1;
  for (int i = 0; i < layers.size(); i++)
  {
    // weight * number_of_nodes
    total *= (2 * layers[i].second);
  }
  //printf("total=%i\n", total);

  PGraph<InterruptiblePThread<SPMMDevice>> graph;
  Nodes inputs;
  Nodes all_nodes;
  init_graph(graph, layers, inputs, all_nodes, trigger_time);

  HostLink hostLink;
  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);
  // Load code and trigger execution
  hostLink.boot("build/code.v", "build/data.v");
  // Trigger execution
  hostLink.go();

  auto b = benchmark::RegisterBenchmark("standard test",
                               [&hostLink, &inputs, &all_nodes, total](auto &st) {
                                 bench_tinsel(st, hostLink, inputs, all_nodes, total);
                               });
  b->Args({num_layers, layer_size, trigger_time});
  b->Unit(benchmark::kMicrosecond);
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  return 0;
}
