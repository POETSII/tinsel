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

void bench_tinsel(benchmark::State &st, HostLink &hostLink,
                  const std::vector<uint32_t> &inputAddrs, const std::vector<uint32_t> &allNodes, int solution)
{
  const int init_value = 1;

  int value = 0;        // init_value;
  int target_value = 0; // solution;

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

      if(DEBUG_VERBOSITY > 0) {
        printf("HOST: Sending value %i to ptid=0x%x uc=0x%x\n", v, ptid,
             send_msg.update_ts);
      }
      // needs to be a thread address
      hostLink.send(ptid, 1, &send_msg);
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
    int x = 0;
    std::chrono::high_resolution_clock::time_point last_recv = std::chrono::high_resolution_clock::now() - std::chrono::seconds(10);
    bool last_recv_done = false;
    auto extra_time = std::chrono::microseconds(st.range(0));
    const bool enable_poke = true;

    while(true) {
      if(true or DEBUG_VERBOSITY > 1) {
        hostLink.pollStdOut();
      }
      
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
      
      bool h = hostLink.canRecv();
      if (h)
      {
        hostLink.recvMsg(&recv_msg, sizeof(SPMMMessage));
        last_recv = now;
        last_recv_done = false;

        if(DEBUG_VERBOSITY > 0) {
          printf("HOST: Received output dest=%i exp=%i v=%i src=0x%x "
                "last_update=0x%x\n",
                recv_msg.dest, target_value, recv_msg.value, recv_msg.src,
                recv_msg.update_ts);
        }

        if (recv_msg.value == target_value)
        {
          // printf("HOST: Value is correct\n");
          break;
        }
      }
      x++;
    }
    // usleep(10000);
    // printf("finished an iteration of the bench\n");
  }

  // printf("finished iterations\n");
}

template <typename T>
void init_hostlink(HostLink &hostLink, T &graph,
              std::vector<std::pair<SPMMDevice::Type, int>> &layer_sizes, std::vector<uint32_t>& inputs, std::vector<uint32_t>& all_nodes)
{
  // Create POETS graph
  const int init_weight = 2;

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

    printf("Creating layer of size=%i\n", layer.second);
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

  for (auto &l : layers)
  {
    for (auto pdid : l.devices)
    {
      graph.devices[pdid]->type = l.type;
      graph.devices[pdid]->init_weight = init_weight;
    }
  }

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("build/code.v", "build/data.v");

  // Trigger execution
  hostLink.go();

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
  for(int i = 0; i < 3; i++) {
    layers.push_back({SPMMDevice::MIDDLE, 5});
  }
  layers.push_back({SPMMDevice::OUTPUT, 1});
  setlinebuf(stdout);

  int total = 1;
  for (int i = 0; i < layers.size(); i++)
  {
    // weight * number_of_nodes
    total *= (2 * layers[i].second);
  }
  printf("total=%i\n", total);

  HostLink hostLink;
  PGraph<SPMMDevice, SPMMMessage> graph;

  std::vector<uint32_t> inputs;
  std::vector<uint32_t> all_nodes;
  init_hostlink(hostLink, graph, layers, inputs, all_nodes);

  auto b = benchmark::RegisterBenchmark("standard test",
                               [&hostLink, &inputs, &all_nodes, total](auto &st) {
                                 bench_tinsel(st, hostLink, inputs, all_nodes, total);
                               });
  b->Range(1000000, 1000000);
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  return 0;
}
