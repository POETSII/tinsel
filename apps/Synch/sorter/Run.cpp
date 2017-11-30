#include <HostLink.h>
#include <Synch.h>
#include "Sorter.h"

// Batcher's bitonic merge-sorting network (any power-of-2 size)
struct BitonicMergeSort {
  // POETS graph being constructed
  PGraph<TwoSorterDevice, TwoSorterMsg> graph;

  // Source and sink devices
  Seq<PDeviceId> sources;
  Seq<PDeviceId> sinks;

  // Two sorter
  void twoSorter(PGlobalPinId in1, PGlobalPinId in2,
                 PGlobalPinId* out1, PGlobalPinId* out2) {
    PDeviceId d = graph.newDevice();
    // Connect inputs to device
    graph.addEdge(in1.devId, in1.pinId, d, 1);
    graph.addEdge(in2.devId, in2.pinId, d, 2);
    // Create two output pins, each one message wide
    graph.setPinWidth(d, 1, 1);
    graph.setPinWidth(d, 2, 1);
    out1->devId = d; out1->pinId = 1;
    out2->devId = d; out2->pinId = 2;
  }

  // Butterfly network
  void butterfly(int n, PGlobalPinId* ins, PGlobalPinId* outs) {
    int mid = 1 << (n-1);
    if (n > 1) {
      PGlobalPinId* inter = new PGlobalPinId [1 << n];
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
  void reverse(int n, PGlobalPinId* ins) {
    int end = 1 << n;
    for (int i = 0, j = end-1; i < j; i++, j--) {
      PGlobalPinId tmp = ins[i];
      ins[i] = ins[j];
      ins[j] = tmp;
    }
  }

  // Bitonic sorting network
  void sorter(int n, PGlobalPinId* ins, PGlobalPinId* outs) {
    if (n == 0)
      outs[0] = ins[0];
    else {
      PGlobalPinId* inter = new PGlobalPinId [1 << n];
      int mid = 1 << (n-1);
      sorter(n-1, ins, inter);
      sorter(n-1, &ins[mid], &inter[mid]);
      reverse(n-1, &inter[mid]);
      butterfly(n, inter, outs);
      delete [] inter;
    }
  }

  // Constructor
  BitonicMergeSort(int n) {
    // Allocate inputs and outputs
    PGlobalPinId* ins = new PGlobalPinId [1 << n];
    PGlobalPinId* outs = new PGlobalPinId [1 << n];
    // Add input-generating devices
    for (int i = 0; i < (1 << n); i++) {
      PDeviceId d = graph.newDevice();
      graph.setPinWidth(d, 1, 1);
      ins[i].devId = d; ins[i].pinId = 1;
      sources.append(d);
    }
    // Create sorting network
    sorter(n, ins, outs);
    // Add output-consuming devices
    for (int i = 0; i < (1 << n); i++) {
      PDeviceId d = graph.newDevice();
      graph.setPinWidth(d, 1, 1);
      graph.addEdge(outs[i].devId, outs[i].pinId, d, 1);
      sinks.append(d);
    }
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
  }

  // Sample array to sort (for testing purposes)
  uint32_t array[] = { 1, 5, 3, 4, 2, 6, 0, 7 };

  // Mark source devices & specify input array to sort
  for (int i = 0; i < sorter.sources.numElems; i++) {
    PDeviceId d = sorter.sources.elems[i];
    graph->devices[d]->kind = SOURCE;
    graph->getState(d)->vals[0] = array[i];
  }

  // Mark sink devices
  for (int i = 0; i < sorter.sinks.numElems; i++) {
    PDeviceId d = sorter.sinks.elems[i];
    graph->getState(d)->vals[1] = i;
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
