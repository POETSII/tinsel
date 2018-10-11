#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <HostLink.h>
#include <POLite.h>
#include <random>
#include <algorithm>

#include "spmm.h"

int main()
{
  const int num_layers = 7;

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<SPMMDevice, SPMMMessage> graph;

  struct Layer {
    SPMMDevice::Type type;
    std::vector<PDeviceId> devices;
  };
  std::vector<Layer> layers;
  
  for(int x = 0; x < num_layers; x++) {
    layers.push_back(Layer());
    int num_nodes = 1;
    Layer& l  = layers.back();
    
    if(x == 0) 
    {
      l.type = SPMMDevice::Type::INPUT;
      num_nodes = 1;
    } 
    else if(x != num_layers - 1) 
    {
      l.type = SPMMDevice::Type::MIDDLE;
      num_nodes = 2;
    } 
    else 
    {
      l.type = SPMMDevice::Type::OUTPUT;
      num_nodes = 1;
    }

    for(int i = 0; i < num_nodes; i++) {
      l.devices.push_back(graph.newDevice());
    }
  }

  for(int x = 1; x < num_layers; x++) {
    Layer& l  = layers[x];
    for(auto pdid_this : l.devices) {
      for(auto pdid_prev : layers[x-1].devices) {
        graph.addEdge(pdid_prev, 0, pdid_this);
      }
    }
  }

  // Prepare mapping from graph to hardware
  graph.map();
            

  std::default_random_engine generator;
  std::uniform_real_distribution<RING_TYPE> distribution(0,2);

  for(auto& l : layers) {
    for(auto pdid : l.devices) {
      graph.devices[pdid]->type = l.type;  
      graph.devices[pdid]->init_weight = distribution(generator); 
    }
  }

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

  for(auto pdid : layers[0].devices){
    uint32_t addr = graph.toDeviceAddr[pdid];
    
    SPMMMessage msg;
    msg.dest = getPLocalDeviceAddr(addr); // needs to be a local thread address
    msg.value = 1; //distribution(generator);
    msg.src = 0;

    // needs to be a thread address
    hostLink.send(getPThreadId(addr), 1, &msg); 
    printf("HOST: Sent message to 0x%x\n", addr);
  }
  
  SPMMMessage recv_msg;

  while(true) {
    bool r = hostLink.pollStdOut();
    if(r) {
      //printf("got data from stdout\n");
    } else {
      usleep(1000);
    }

    bool h = hostLink.canRecv();
    if(h) {
      hostLink.recv(&recv_msg);
      printf("HOST: Received output dest=%i v=%f src=0x%x\n", recv_msg.dest, recv_msg.value, recv_msg.src);
    }
  }

  // Wait for response


  printf("Done\n");

  // Get finish time
  gettimeofday(&finish, NULL);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
