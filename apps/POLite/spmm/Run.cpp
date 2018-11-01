#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "spmm.h"

#include <EdgeList.h>
#include <HostLink.h>
#include <POLite.h>
#include <fstream>

#include <benchmark/benchmark.h>

int counter = 1;
bool toggle_flag = true;
const int HOST_SOURCE = 0;

using Nodes = std::vector<PDeviceAddr>;

template <typename T> class TimeSeriesStore {
  using Clock = std::chrono::system_clock;
  using TimeUnit = std::chrono::microseconds;

public:
  TimeSeriesStore(std::string n) : name(n), begin(Clock::now()) {}
  void add_point(T x) {
    auto now = Clock::now();
    auto td = std::chrono::duration_cast<TimeUnit>(now - begin);
    values.emplace_back(td, x);
  }
  ~TimeSeriesStore() {
    if (!values.empty()) {
      dump();
    }
  }
  void dump() {
    std::ofstream of(name, std::ios::out);
    of << "time"
       << ","
       << "value" << std::endl;
    for (auto &v : values) {
      of << v.first.count() << "," << v.second << std::endl;
    }
  }

private:
  std::string name;
  Clock::time_point begin;
  std::vector<std::pair<std::chrono::microseconds, T>> values;
};

void seedInputs(HostLink &hostLink, const Nodes &targets, int v) {
  PMessage<None, SPMMMessage> send_msg;

  for (PDeviceAddr addr : targets) {
    if (DEBUG_VERBOSITY > 1) {
      printf("HOST: Sending value_update %i to thread_id=0x%x dev_id=0x%x "
             "uc=0x%x\n",
             v, addr.threadId, addr.devId, send_msg.payload.update_ts);
    }

    send_msg.devId = addr.devId;
    send_msg.payload.type = SPMMMessage::VALUE_UPDATE;
    send_msg.payload.value = v; // distribution(generator);
    send_msg.payload.src = HOST_SOURCE;
    send_msg.payload.update_ts = counter++;

    hostLink.send(addr.threadId, 2, &send_msg);
  }
};

void seedWeights(HostLink &hostLink, const Nodes &targets, int v) {
  PMessage<None, SPMMMessage> send_msg;

  for (PDeviceAddr addr : targets) {
    if (DEBUG_VERBOSITY > 1) {
      printf("HOST: Sending WEIGHT_UPDATE %i to thread_id=0x%x dev_id=0x%x "
             "uc=0x%x\n",
             v, addr.threadId, addr.devId, send_msg.payload.update_ts);
    }
    send_msg.devId = addr.devId;
    send_msg.payload.type = SPMMMessage::WEIGHT_UPDATE;
    send_msg.payload.value = v;
    send_msg.payload.src = HOST_SOURCE;
    hostLink.send(addr.threadId, 2, &send_msg);
  }
};

void seedExtraEdge(HostLink &hostLink, const Nodes &sources,
                   PDeviceAddr target) {
  // go straight from input to output
  PMessage<None, SPMMMessage> send_msg;

  for (PDeviceAddr addr : sources) {
    if (DEBUG_VERBOSITY > 1) {
      printf("HOST: Sending ADD_EDGE to thread_id=0x%x dev_id=0x%x "
             "uc=0x%x\n",
             addr.threadId, addr.devId, send_msg.payload.update_ts);
    }
    send_msg.devId = addr.devId;
    send_msg.payload.type = SPMMMessage::ADD_EDGE;
    send_msg.payload.update_ts = 0;
    send_msg.payload.src = target.num();
    hostLink.send(addr.threadId, 2, &send_msg);
  }
};

void bench_tinsel(benchmark::State &st, HostLink &hostLink,
                  const Nodes &inputAddrs, const Nodes &outputAddrs,
                  RING_TYPE solution) {
  const RING_TYPE init_value = 1;

  RING_TYPE value = 0;        // init_value;
  RING_TYPE target_value = 0; // solution;

  PMessage<None, SPMMMessage> recv_msg;
  TimeSeriesStore<RING_TYPE> ts{"output/values.csv"};

  for (auto _ : st) {
    // switch the expected value
    if (toggle_flag) {
      value = init_value;
      target_value = solution;
    } else {
      value = 0;
      target_value = 0;
    }
    toggle_flag = !toggle_flag;

    //
    // send messages to all the inputs
    seedInputs(hostLink, inputAddrs, value);

    // seedExtraEdge(hostLink, inputAddrs, outputAddrs[0]);

    // seedWeights(hostLink, inputAddrs, 3);

    while (true) {
      if (true or DEBUG_VERBOSITY > 1) {
        hostLink.pollStdOut();
      }

      bool h = hostLink.canRecv();
      if (h) {
        hostLink.recvMsg(&recv_msg, sizeof(PMessage<None, SPMMMessage>));
        if (DEBUG_VERBOSITY > 0) {

          if constexpr (std::is_same<RING_TYPE, float>::value) {
            printf("HOST: Received output dest=%i exp=%f v=%f src=0x%x "
                   "last_update=%x %i\n",
                   recv_msg.devId, target_value, recv_msg.payload.value,
                   recv_msg.payload.src, recv_msg.payload.update_ts,
                   recv_msg.payload.update_ts);
          } else {
            printf("HOST: Received output dest=%i exp=%i v=%i src=0x%x "
                   "last_update=%x %i\n",
                   recv_msg.devId, target_value, recv_msg.payload.value,
                   recv_msg.payload.src, recv_msg.payload.update_ts,
                   recv_msg.payload.update_ts);
          }
        }

        ts.add_point(recv_msg.payload.value);

        if (recv_msg.payload.value == target_value) {
          if (DEBUG_VERBOSITY > 0) {
            printf("HOST: Value is correct\n");
          }
          break;
        }
      }
    }
  }
}

void bench_tinsel_push(benchmark::State &st, HostLink &hostLink,
                       const Nodes &inputAddrs, const Nodes &outputAddrs,
                       RING_TYPE solution, std::string edges) {
  const RING_TYPE init_value = 1;
  const int NUM_SWITCHES = 1000;

  RING_TYPE value = 0;        // init_value;
  RING_TYPE target_value = 0; // solution;

  std::cout << "[";
  for (int t = 300; t < 2000; t += 100) {
    std::chrono::microseconds settle_time{t};
    std::cout << settle_time.count() << ", ";

    PMessage<None, SPMMMessage> recv_msg;
    TimeSeriesStore<RING_TYPE> ts{"output/values_" + edges + "_" +
                                  std::to_string(settle_time.count()) + ".csv"};

    // keep increasing the switching frequency until the outputs are no longer
    // consistent
    std::atomic<bool> stop = false;

    std::thread switcher{[&]() {
      bool local_toggle_flag = false;
      for (int i = 0; i < NUM_SWITCHES; i++) {
        if (local_toggle_flag) {
          value = init_value;
          target_value = solution;
        } else {
          value = 0;
          target_value = 0;
        }
        local_toggle_flag = !local_toggle_flag;

        // send messages to all the inputs
        seedInputs(hostLink, inputAddrs, value);

        std::this_thread::sleep_for(settle_time);
      }
      stop = true;
    }};

    while (!stop) {
      if (DEBUG_VERBOSITY > 1) {
        hostLink.pollStdOut();
      }

      bool h = hostLink.canRecv();
      if (h) {
        hostLink.recvMsg(&recv_msg, sizeof(PMessage<None, SPMMMessage>));

        if (DEBUG_VERBOSITY > 0) {
          std::cout << "HOST: Received output "
                    << " dest=" << recv_msg.devId << " exp=" << target_value
                    << " v=" << recv_msg.payload.value
                    << " src=" << recv_msg.payload.src
                    << " last_update=" << recv_msg.payload.update_ts
                    << std::endl;
        }

        ts.add_point(recv_msg.payload.value);
      }
    }

    switcher.join();
  }
  std::cout << "]" << std::endl;
  std::cout << " ----------------------------------- DONE --------------------------" << std::endl;
}

struct ThreadIdLayout {
  uint16_t boardY : TinselMeshYBits;      // 2
  uint16_t boardX : TinselMeshXBits;      // 2
  uint16_t boxY : TinselMailboxMeshYBits; // 2
  uint16_t boxX : TinselMailboxMeshXBits; // 2
  uint16_t threadNum : 6;                 // 2 + 4
};

template <typename T> void dump_graph(T &graph) {

  FILE *out = stderr;

  auto printLocation = [&graph, out](PDeviceId pdid) {
    PDeviceAddr pdaddr = graph.toDeviceAddr[pdid];
    PLocalDeviceId pldaddr = pdaddr.devId;
    PThreadId ptid = pdaddr.threadId;
    union C {
      ThreadIdLayout l;
      PThreadId ptid;
    } c;
    c.ptid = ptid;
    fprintf(out, "%u,%u,%u,%u,%u,%u", c.l.boardY, c.l.boardX, c.l.boxY,
            c.l.boxX, c.l.threadNum, pldaddr);
  };

  fprintf(
      out,
      "fromBoardY,fromBoardX,fromBoxY,fromBoxX,fromThreadNum,fromDeviceNum,");
  fprintf(out, "toBoardY,toBoardX,toBoxY,toBoxX,toThreadNum,toDeviceNum\n");

  for (NodeId start_node = 0; start_node < graph.graph.outgoing->numElems;
       start_node++) {
    Seq<NodeId> *end_nodes = graph.graph.outgoing->elems[start_node];

    for (NodeId end_node_idx = 0; end_node_idx < end_nodes->numElems;
         end_node_idx++) {
      NodeId end_node = end_nodes->elems[end_node_idx];

      printLocation(start_node);
      fprintf(out, ",");
      printLocation(end_node);
      fprintf(out, "\n");
    }
  }
}

int main(int argc, char **argv) {
  setlinebuf(stdout);

  assert(argc >= 4 && "argv[0] [edges_file] [trigger_time] [result]");
  std::string edges = std::string(argv[1]);
  const int trigger_time = std::atoi(argv[2]);
  const double total = std::atof(argv[3]);

  std::string edge_file = "input/" + edges + ".txt";
  printf("Loading in the graph...");
  fflush(stdout);
  EdgeList net;
  net.read(edge_file.c_str());
  printf(" done\n");
  printf("Max fan-out = %d\n", net.maxFanOut());

  PGraph<InterruptiblePThread<SPMMDevice>> graph;

  Nodes inputs;
  Nodes outputs;
  {
    // Create nodes in POETS graph
    for (uint32_t i = 0; i < net.numNodes; i++) {
      PDeviceId id = graph.newDevice();
      assert(i == id);
    }

    // Create connections in POETS graph
    for (uint32_t i = 0; i < net.numNodes; i++) {
      uint32_t numNeighbours = net.neighbours[i][0];
      for (uint32_t j = 0; j < numNeighbours; j++)
        graph.addEdge(i, 0, net.neighbours[i][j + 1]);
    }

    // std::mt19937 e;
    // std::normal_distribution<RING_TYPE> normal_dist(0.0625, 0.5);

    // Prepare mapping from graph to hardware
    graph.map();
    // dump_graph(graph);

    for (PDeviceId i = 0; i < graph.numDevices; i++) {
      auto fo = graph.fanOut(i);
      if (fo == 0) {
        graph.devices[i]->state.type = SPMMState::OUTPUT;
        outputs.push_back(graph.toDeviceAddr[i]);
      } else {
        graph.devices[i]->state.type = SPMMState::MIDDLE;
      }

      graph.devices[i]->state.init_weight = 2;
      graph.devices[i]->state.triggerTime = trigger_time;

      auto fi = graph.fanIn(i);
      assert(fi < MAX_FAN_IN && "max fan-in exceeded, please partition graph");
      if (fi == 0) {
        inputs.push_back(graph.toDeviceAddr[i]);
      }
    }
  }
  std::cout << "Total graph size=" << graph.numDevices
            << " inputs=" << inputs.size() << " outputs=" << outputs.size()
            << std::endl;

  HostLink hostLink;
  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);
  // Load code and trigger execution
  hostLink.boot("build/code.v", "build/data.v");
  // Trigger execution
  hostLink.go();

  auto b = benchmark::RegisterBenchmark(edges.c_str(), [&](auto &st) {
    bench_tinsel_push(st, hostLink, inputs, outputs, total, edges);
  });
  // auto b = benchmark::RegisterBenchmark(edges.c_str(), [&](auto &st) {
  //   bench_tinsel(st, hostLink, inputs, outputs, total);
  // });

  b->Args({trigger_time});
  b->Unit(benchmark::kMicrosecond);
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  return 0;
}
