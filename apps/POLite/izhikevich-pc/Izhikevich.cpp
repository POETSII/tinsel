// SPDX-License-Identifier: BSD-2-Clause
// (Based on code by David Thomas)

#include <EdgeList.h>
#include <assert.h>
#include <sys/time.h>
#include "RNG.h"

#define NUM_STEPS 100

// Neuron 
struct Neuron {
  // Random-number-generator state
  uint32_t rng;
  // Neuron state
  float u, v, I, spikeCount;
  // Neuron properties
  float a, b, c, d, Ir;
};

int main(int argc, char**argv)
{
  if (argc != 2) {
    printf("Specify edges file\n");
    exit(EXIT_FAILURE);
  }

  // Read network
  EdgeList net;
  net.read(argv[1]);

  // Ratio of excitatory to inhibitory neurons
  double excitatory = 0.8;

  // Mark each neuron as excitatory (or inhibiatory)
  srand(1);
  bool* excite = new bool [net.numNodes];
  for (int i = 0; i < net.numNodes; i++) {
    excite[i] = urand() < excitatory;
  }

  // Edge weights
  float** weight = new float* [net.numNodes];
  for (int i = 0; i < net.numNodes; i++) {
    uint32_t numEdges = net.neighbours[i][0];
    weight[i] = new float [numEdges];
    for (int j = 0; j < numEdges; j++) {
      weight[i][j] = excite[i] ? 0.5 * urand() : -urand();
    }
  }

  // State for each neuron
  srand(2);
  Neuron* neuron = new Neuron [net.numNodes];
  for (int i = 0; i < net.numNodes; i++) {
    Neuron* n = &neuron[i];
    n->rng = (int32_t) (urand()*((double) (1<<31)));
    if (excite[i]) {
      float re = (float) urand();
      n->a = 0.02;
      n->b = 0.2;
      n->c = -65+15*re*re;
      n->d = 8-6*re*re;
      n->Ir = 5;
    }
    else {
      float ri = (float) urand();
      n->a = 0.02+0.08*ri;
      n->b = 0.25-0.05*ri;
      n->c = -65;
      n->d = 2;
      n->Ir = 2;
    }
  }

  // Spike array
  bool* spike = new bool [net.numNodes];

  // Initialisation
  for (int i = 0; i < net.numNodes; i++) {
    Neuron* n = &neuron[i];
    n->v = -65.0;
    n->u = n->b * n->v;
    n->I = n->Ir * grng(n->rng);
  }

  // Timer
  printf("Started\n");
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  // Simulation
  int64_t totalSpikes = 0;
  for (int t = 0; t <= NUM_STEPS; t++) {
    // Update state
    for (int i = 0; i < net.numNodes; i++) {
      spike[i] = false;
      Neuron* n = &neuron[i];
      float &v = n->v;
      float &u = n->u;
      float &I = n->I;
      v = v+0.5*(0.04*v*v+5*v+140-u+I); // Step 0.5 ms
      v = v+0.5*(0.04*v*v+5*v+140-u+I); // for numerical
      u = u + n->a*(n->b*v-u);          // stability
      if (v >= 30.0) {
        n->v = n->c;
        n->u += n->d;
        spike[i] = true;
      }
      n->I = n->Ir * grng(n->rng);
    }
    // Update I-values
    uint32_t spikes = 0;
    for (int i = 0; i < net.numNodes; i++) {
      Neuron* n = &neuron[i];
      if (spike[i]) {
        spikes++;
        n->spikeCount++;
        uint32_t numEdges = net.neighbours[i][0];
        uint32_t* dst = &net.neighbours[i][1];
        for (int j = 0; j < numEdges; j++) {
          neuron[dst[j]].I += weight[i][j];
        }
      }
    }
    //printf("%d: %d\n", t, spikes);
    totalSpikes += spikes;
  }
  gettimeofday(&finish, NULL);

  printf("Total spikes: %ld\n", totalSpikes);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
