#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "spmm.h"

#include <EdgeList.h>
#include <HostLink.h>
#include <POLite.h>

#include <benchmark/benchmark.h>

int counter = 1;
bool toggle_flag = true;

using Nodes = std::vector<PDeviceAddr>;

void bench_tinsel(benchmark::State &st, HostLink &hostLink,
                  const Nodes &inputAddrs, const Nodes &outputAddrs,
                  RING_TYPE solution) {
  const RING_TYPE init_value = 1;

  RING_TYPE value = 0;        // init_value;
  RING_TYPE target_value = 0; // solution;

  PMessage<None, SPMMMessage> recv_msg;
  PMessage<None, SPMMMessage> push_msg;
  push_msg.payload.update_ts = -1;

  int HOST_SOURCE = 0;

  auto seedInputs = [&](int v) {
    PMessage<None, SPMMMessage> send_msg;

    for (PDeviceAddr addr : inputAddrs) {
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

  auto seedWeights = [&](int v) {
    PMessage<None, SPMMMessage> send_msg;

    for (PDeviceAddr addr : inputAddrs) {
      if (DEBUG_VERBOSITY > 1) {
        printf("HOST: Sending weight_update %i to thread_id=0x%x dev_id=0x%x "
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

  auto seedExtraEdge = [&](PDeviceAddr target) {
    // go straight from input to output
    PMessage<None, SPMMMessage> send_msg;

    for (PDeviceAddr addr : inputAddrs) {
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

    seedExtraEdge(outputAddrs[0]);

    // send messages to all the inputs
    seedInputs(value);

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
  const char *edges = argv[1];
  const int trigger_time = std::atoi(argv[2]);
  const double total = std::atof(argv[3]);

  printf("Loading in the graph...");
  fflush(stdout);
  EdgeList net;
  net.read(edges);
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

    // Create POETS graph
    const RING_TYPE init_weight = 2;

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

      graph.devices[i]->state.init_weight = init_weight;
      graph.devices[i]->state.triggerTime = trigger_time;

      auto fi = graph.fanIn(i);
      assert(fi <= MAX_FAN_IN && "max fan-in exceeded, please partition graph");
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

  auto b = benchmark::RegisterBenchmark(edges, [&](auto &st) {
    bench_tinsel(st, hostLink, inputs, outputs, total);
  });
  b->Args({trigger_time});
  b->Unit(benchmark::kMicrosecond);
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  return 0;
}
