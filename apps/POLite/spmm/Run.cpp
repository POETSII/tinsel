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

using Nodes = std::vector<uint32_t>;
using LayerSizes = std::vector<std::pair<SPMMDevice::Type, int>>;

void bench_tinsel(benchmark::State &st, HostLink &hostLink, const Nodes& inputAddrs, const Nodes& allNodes, RING_TYPE solution)
{
  const RING_TYPE init_value = 1;

  RING_TYPE value = 0;        // init_value;
  RING_TYPE target_value = 0; // solution;

  SPMMMessage recv_msg;
  SPMMMessage push_msg;
  push_msg.update_ts = -1;

  auto seedInputs = [&inputAddrs, &hostLink](int v) {
    SPMMMessage send_msg;

    for (auto addr : inputAddrs){
      // needs to be a local thread address
      send_msg.dest = getPLocalDeviceAddr(addr);
      send_msg.value = v; // distribution(generator);
      send_msg.src = 0;
      send_msg.update_ts = counter++; // higher than everything but not triggering

      auto ptid = getPThreadId(addr);

      if(DEBUG_VERBOSITY > 1) {
        printf("HOST: Sending value %i to ptid=0x%x uc=0x%x\n", v, ptid,
             send_msg.update_ts);
      }
      // needs to be a thread address
      hostLink.send(ptid, 1, &send_msg);
      //break;
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

    // wait until we get the correct response
    // int x = 0;
    // std::chrono::high_resolution_clock::time_point last_recv = std::chrono::high_resolution_clock::now() + std::chrono::seconds(6000);
    // bool last_recv_done = false;
    // auto extra_time = std::chrono::microseconds(st.range(0));
    // const bool enable_poke = false;

    while(true) {
      if(true or DEBUG_VERBOSITY > 1) {
        hostLink.pollStdOut();
      }
      
      
      /*
      // Poking threads should not be necessary
      auto now = std::chrono::high_resolution_clock::now();
      if(enable_poke and !last_recv_done and now > last_recv + extra_time) {
        if(DEBUG_VERBOSITY > 0) {
          printf("HOST: Sending pokes to threads\n");
        }
        last_recv_done = true;

        // also update the inputs in case of packet loss/sends that weren't done
        // seedInputs(value);

        // poke the threads to send out their updates 
        for(auto addr : allNodes) {
          push_msg.dest = getPLocalDeviceAddr(addr);
          auto tid = getPThreadId(addr);
          hostLink.send(tid, 1, &push_msg);
        }
      }
      */

      bool h = hostLink.canRecv();
      if (h)
      {
        hostLink.recvMsg(&recv_msg, sizeof(SPMMMessage));
        // last_recv = now;
        // last_recv_done = false;

        if(DEBUG_VERBOSITY > 0) {

          if constexpr (std::is_same<RING_TYPE, float>::value) {
            printf("HOST: Received output dest=%i exp=%f v=%f src=0x%x "
                "last_update=%x %i\n",
                recv_msg.dest, target_value, recv_msg.value, recv_msg.src,
                recv_msg.update_ts, recv_msg.update_ts);
          } else {
            printf("HOST: Received output dest=%i exp=%i v=%i src=0x%x "
                "last_update=%x %i\n",
                recv_msg.dest, target_value, recv_msg.value, recv_msg.src,
                recv_msg.update_ts, recv_msg.update_ts);
          }
          
        }

        if (recv_msg.value == target_value)
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
    //printf("pdid=%x addr=%x\n", pdid, pdaddr);

    PLocalDeviceAddr pldaddr = getPLocalDeviceAddr(pdaddr);
    PThreadId ptid = getPThreadId(pdaddr);

    //printf("pldaddr=%x ptid=%x %x %x\n", pldaddr, ptid, sizeof(PThreadId), sizeof(ThreadIdLayout));



    //static_assert(sizeof(PThreadId) == sizeof(ThreadIdLayout));

    union C {
      ThreadIdLayout l;
      PThreadId ptid;
    } c;
    c.ptid = ptid;
    //printf("boardY=%x boardX=%x boxY=%x boxX=%x threadNum=%x\n", c.l.boardY, c.l.boardX, c.l.boxY, c.l.boxX, c.l.threadNum);
    
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

  for(PDeviceId pdid = 0; pdid < graph.numDevices; pdid++) {

    
    // ThreadIdLayout t;
    // t = (ThreadIdLayout)ptid;
    // uint32_t threadId = boardY;
    // threadId = (threadId << TinselMeshXBits) | boardX;
    // threadId = (threadId << TinselMailboxMeshYBits) | boxY;
    // threadId = (threadId << TinselMailboxMeshXBits) | boxX;
    // threadId = (threadId << (TinselLogCoresPerMailbox +
    //               TinselLogThreadsPerCore)) | threadNum;
  }
}

template <typename T>
void init_graph(T &graph, LayerSizes &layer_sizes, Nodes& inputs, Nodes& all_nodes, int trigger_time)
{
  // Create POETS graph
  const RING_TYPE init_weight = 2;

  struct Layer
  {
    Layer(SPMMDevice::Type t) : type(t) {}
    SPMMDevice::Type type;
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
  graph.map([trigger_time](PGraph<SPMMDevice, SPMMMessage>::ThreadType * t){
    t->triggerTime = trigger_time;
  });

  dump_graph(graph);

  for (auto &l : layers)
  {
    for (auto pdid : l.devices)
    {
      graph.devices[pdid]->type = l.type;
      graph.devices[pdid]->init_weight = init_weight;
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
  std::vector<std::pair<SPMMDevice::Type, int>> layers;
  
  const int num_layers = std::atoi(argv[1]);
  const int layer_size = std::atoi(argv[2]);
  const int trigger_time = std::atoi(argv[3]);
  
  for(int i = 0; i < num_layers; i++) {
    layers.push_back({SPMMDevice::MIDDLE, layer_size});
  }
  layers.push_back({SPMMDevice::OUTPUT, 1});
  setlinebuf(stdout);

  int total = 1;
  for (int i = 0; i < layers.size(); i++)
  {
    // weight * number_of_nodes
    total *= (2 * layers[i].second);
  }
  //printf("total=%i\n", total);

  PGraph<SPMMDevice, SPMMMessage> graph;
  std::vector<uint32_t> inputs;
  std::vector<uint32_t> all_nodes;
  init_graph(graph, layers, inputs, all_nodes, trigger_time);

  return 0;

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
