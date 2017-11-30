#include <HostLink.h>
#include <POLite.h>
#include "Sorter.h"

// A pin is a device id and a pin number
struct Pin {
  PDeviceId dev;
  int pin;
};

// A connection from a pin to a device
struct Connection {
  Pin source;
  PDeviceId sink;
  Connection() {}
  Connection(Pin p, PDeviceId d) { source = p; sink = d; }
};

// Batcher's bitonic merge-sorting network (any power-of-2 size)
struct BitonicMergeSort {
  // POETS graph being constructed
  PGraph<TwoSorterDevice, TwoSorterMsg> graph;

  // Intermediate list of connections between two sorters
  Seq<Connection> connections;

  // Source and sink devices
  Seq<PDeviceId> sources;
  Seq<PDeviceId> sinks;

  // Two sorter
  void twoSorter(Pin in1, Pin in2, Pin* out1, Pin* out2) {
    PDeviceId d = graph.newDevice();
    connections.append(Connection(in1, d));
    connections.append(Connection(in2, d));
    out1->dev = d; out1->pin = 0;
    out2->dev = d; out2->pin = 1;
  }

  // Butterfly network
  void butterfly(int n, Pin* ins, Pin* outs) {
    int mid = 1 << (n-1);
    if (n > 1) {
      Pin* inter = new Pin [1 << n];
      for (int i = 0, j = mid; i < mid; i++, j++)
        twoSorter(ins[i], ins[j], &inter[i], &inter[j]);
      butterfly(n-1, inter, outs);
      butterfly(n-1, &inter[mid], &outs[mid]);
      delete [] inter;
    }
    else {
      twoSorter(ins[0], ins[1], &outs[0], &outs[1]);
    }
  }

  // Reverse a power-of-2-length array
  void reverse(int n, Pin* ins) {
    int end = 1 << n;
    for (int i = 0, j = end-1; i < j; i++, j--) {
      Pin tmp = ins[i];
      ins[i] = ins[j];
      ins[j] = tmp;
    }
  }

  // Bitonic sorting network
  void sorter(int n, Pin* ins, Pin* outs) {
    if (n == 0)
      outs[0] = ins[0];
    else {
      Pin* inter = new Pin [1 << n];
      int mid = 1 << (n-1);
      sorter(n-1, ins, inter);
      sorter(n-1, &ins[mid], &inter[mid]);
      reverse(n-1, &inter[mid]);
      butterfly(n, inter, outs);
      delete [] inter;
    }
  }

  // Add all connections with the given pin number to the POETS graph
  void addConnections(int n, int pin) {
    for (int i = 0; i < connections.numElems; i++) {
      Connection c = connections.elems[i];
      if (c.source.pin == pin) graph.addEdge(c.source.dev, c.sink);
    }
  }

  // Constructor
  BitonicMergeSort(int n) {
    // Allocate inputs and outputs
    Pin* ins = new Pin [1 << n];
    Pin* outs = new Pin [1 << n];
    // Add input-generating devices
    for (int i = 0; i < (1 << n); i++) {
      PDeviceId d = graph.newDevice();
      ins[i].dev = d; ins[i].pin = 0;
      sources.append(d);
    }
    // Create sorting network
    sorter(n, ins, outs);
    // Add output-consuming devices
    for (int i = 0; i < (1 << n); i++) {
      PDeviceId d = graph.newDevice();
      connections.append(Connection(outs[i], d));
      sinks.append(d);
    }
    // Add all pin 0 connections to POETS graph
    addConnections(n, 0);
    // Add all pin 1 connections to POETS graph
    addConnections(n, 1);
    // Release inputs and outputs
    delete [] ins;
    delete [] outs;
  }
};

int main()
{
  // Log of the size of the sorting network
  const uint32_t logSize = 3;

  // Connection to tinsel machine
  HostLink hostLink;

  // Create sorting network
  BitonicMergeSort sorter(logSize);

  // Prepare mapping from graph to hardware
  PGraph<TwoSorterDevice, TwoSorterMsg>* graph = &sorter.graph;
  graph->map();

  // Initialise devices
  for (PDeviceId i = 0; i < graph->numDevices; i++) {
    graph->devices[i]->kind = TWO_SORTER;
    graph->devices[i]->time = 8;
  }

  // Sample array to sort (for testing purposes)
  uint32_t array[] = { 1, 5, 3, 4, 2, 6, 0, 7 };

  // Mark source devices & specify input array to sort
  for (int i = 0; i < sorter.sources.numElems; i++) {
    PDeviceId d = sorter.sources.elems[i];
    graph->devices[d]->kind = SOURCE;
    graph->devices[d]->vals[0] = array[i];
  }

  // Mark sink devices
  for (int i = 0; i < sorter.sinks.numElems; i++) {
    PDeviceId d = sorter.sinks.elems[i];
    graph->devices[d]->id = i;
    graph->devices[d]->kind = SINK;
  }

  // Write graph down to tinsel machine via HostLink
  graph->write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();

  // Receive results
  uint32_t result[1 << logSize];
  for (int i = 0; i < (1 << logSize); i++) {
    TwoSorterMsg msg;
    hostLink.recvMsg(&msg, sizeof(TwoSorterMsg));
    result[msg.id] = msg.val;
  }

  // Print results
  for (int i = 0; i < (1 << logSize); i++)
    printf("%d\n", result[i]);
  
  return 0;
}
