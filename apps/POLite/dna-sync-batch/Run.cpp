// SPDX-License-Identifier: BSD-2-Clause
#include "impute.h"

#include <HostLink.h>
#include <POLite.h>
#include <EdgeList.h>
#include <sys/time.h>

/*****************************************************
 * Genomic Imputation - Batch Processing Version
 * ***************************************************
 * This code streams target haplotypes from the X86 side
 * USAGE:
 * To Be Completed ...
 * 
 * PLEASE NOTE:
 * To Be Completed ...
 * 
 * ssh jordmorr@ayres.cl.cam.ac.uk
 * scp -r C:\Users\drjor\Documents\tinsel\apps\POLite\dna-sync-batch jordmorr@ayres.cl.cam.ac.uk:~/tinsel/apps/POLite
 * scp jordmorr@ayres.cl.cam.ac.uk:~/tinsel/apps/POLite/dna-sync-batch/build/stats.txt C:\Users\drjor\Documents\tinsel\apps\POLite\dna-sync-batch
 * ****************************************************/

int main(int argc, char **argv)
{
  // Number of devices per column
  uint32_t colHeight = 8;
  // Number of columns
  uint32_t numCols = 768;
  // Number of waves to inject;
  uint32_t numWaves = 10000;

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<ImpDevice, ImpState, None, ImpMessage> graph;

  // Create devices
  for (uint32_t i = 0; i < numCols * colHeight; i++) {
    PDeviceId id = graph.newDevice();
    assert(i == id);
  }

  // Neighbouring columns are fully connected
  for (uint32_t col = 0; col < numCols-1; col++) {
    uint32_t srcCol = col * colHeight;
    uint32_t dstCol = (col+1) * colHeight;
    for (uint32_t i = 0; i < colHeight; i++) {
      for (uint32_t j = 0; j < colHeight; j++) {
        graph.addEdge(srcCol+i, 0, dstCol+j);
      }
    }
  }

  // Prepare mapping from graph to hardware
  graph.map();

  // Initialise devices
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    graph.devices[i]->state.didRecv = 0;
    if (i < colHeight)
      graph.devices[i]->state.kind = Produce;
    else if (i >= (numCols-1) * colHeight)
      graph.devices[i]->state.kind = Consume;
    else
      graph.devices[i]->state.kind = Forward;
    graph.devices[i]->state.numWaves = numWaves;
  }

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();
  printf("Starting\n");

  // Consume performance stats
  politeSaveStats(&hostLink, "stats.txt");

  printf("Finished\n");
  return 0;
}
