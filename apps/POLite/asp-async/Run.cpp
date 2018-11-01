#include "ASP.h"

#include <HostLink.h>
#include <POLite.h>
#include <EdgeList.h>
#include <assert.h>
#include <sys/time.h>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include <vector>

int main(int argc, char**argv)
{
  if (argc != 2) {
    printf("Specify edges file\n");
    exit(EXIT_FAILURE);
  }

  // Read network
  EdgeList net;
  net.read(argv[1]);

  // Print max fan-out
  auto maxFanOut = net.maxFanOut();
  //printf("Max fan-out = %d\n", maxFanOut);

  auto msg_size = sizeof(PMessage<None, ASPMessage>);
  //printf("Message size = %d\n", msg_size);

  // Check that parameters make sense
  assert(NUM_SOURCES <= net.numNodes);

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<DefaultPThread<ASPDevice>> graph;

  // Create nodes in POETS graph
  for (uint32_t i = 0; i < net.numNodes; i++) {
    PDeviceId id = graph.newDevice();
    assert(i == id);
  }

  // Create connections in POETS graph
  for (uint32_t i = 0; i < net.numNodes; i++) {
    uint32_t numNeighbours = net.neighbours[i][0];
    for (uint32_t j = 0; j < numNeighbours; j++)
      graph.addEdge(i, 0, net.neighbours[i][j+1]);
  }

  // Prepare mapping from graph to hardware
  graph.map();

  // Initialise devices
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    ASPState* dev = &graph.devices[i]->state;
    dev->id = i;
    //dev->do_done = false;
    //dev->numReached = 0;
    dev->toUpdateIdx = 0;
    //dev->perf.update_slot_overflow = 0;
    dev->sent_result = false;

    for(auto& d : dev->distances) {
      d = INIT_DISTANCE;
    }

    if (i < NUM_SOURCES) {
      // This is a source node
      // By definition, a source node reaches itself with distance 0
      //dev->do_done = 0;
      
      dev->distances[i] = 0;
      dev->toUpdate[0] = i;
      dev->toUpdateIdx = 1;
    }
  }
 
  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("build/code.v", "build/data.v");
  hostLink.go();

  // Timer
  //printf("Started\n");
  std::vector<ASPMessage> results; // use idle detection in software
  results.resize(graph.numDevices);
  assert(results.size() == graph.numDevices);
  using Clock = std::chrono::high_resolution_clock;
  auto start = Clock::now();
  auto last_msg = start;

  int x = 0;
  while(x < graph.numDevices) {
    PMessage<None, ASPMessage> msg;
    
    auto now = Clock::now();
    //if(now - start > std::chrono::seconds(5) or x >= graph.numDevices) {
    if(now - start > std::chrono::milliseconds(3000)) {
      break;
    }

    if(hostLink.canRecv()) {
      x++;
      hostLink.recvMsg(&msg, sizeof(msg));

      auto& oldPayload = results[msg.payload.src];
      auto& newPayload = msg.payload;
      
#ifdef WITH_PERF
      int recv_diff = newPayload.perf.recv - oldPayload.perf.recv;
      int send_diff = newPayload.perf.send_dest_success - oldPayload.perf.send_dest_success;
      int result_diff = newPayload.result - oldPayload.result;
#endif

      if(newPayload.send_time > oldPayload.send_time) {
        oldPayload = newPayload;
        //printf("i=%u size=%u src=%x result=%u new_ts=%u recv_diff=%i send_diff=%i result_diff=%i\n", x, results.size(), newPayload.src, newPayload.result, newPayload.send_time, recv_diff, send_diff, result_diff); // d0=%u d1=%u d2=%u
        printf("i=%u size=%u src=%x result=%u new_ts=%u\n", x, results.size(), newPayload.src, newPayload.result, newPayload.send_time); // d0=%u d1=%u d2=%u
        last_msg = now;
      }
    }
  }



  uint32_t sum = 0;
  uint32_t totalRecv = 0;
  uint32_t totalSent = 0;

  for(auto& p : results) {
    sum += p.result;

#ifdef WITH_PERF
    totalRecv += p.perf.recv;
    totalSent += p.perf.send_dest_success;
#endif

    // std::cout << "src=" << p.src 
    //           << "\tsend_host=" << p.perf.send_host_success 
    //           << "\tsend_dest=" << p.perf.send_dest_success 
    //           << "\ttotal_recv=" << p.perf.recv 
    //           << "\tupdate_overflow=" << p.perf.update_slot_overflow 
    //           << "\tval=" << p.result
    //           << std::endl;
  }

  std::cout << "{" 
            << "\"" << "ns" << "\":" << NUM_SOURCES
            << ",\"" << "x_len" << "\":" << TinselMeshXLen
            << ",\"" << "y_len" << "\":" << TinselMeshYLen
            << ",\"" << "graph" << "\":\"" << argv[1] << "\""
            << ",\"" << "graph_size" << "\":" << graph.numDevices
            << ",\"" << "sum" << "\":" << sum
            << ",\"" << "time" << "\":" << static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(last_msg - start).count())
            << "}"
            << std::endl;



  return 0;
}
