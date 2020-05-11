// SPDX-License-Identifier: BSD-2-Clause
#ifndef _PLACER_H_
#define _PLACER_H_

#include <stdint.h>
#include <metis.h>
#include <POLite/Graph.h>

typedef uint32_t PartitionId;

// Partition and place a graph on a 2D mesh
struct Placer {
  // Select between different methods
  enum Method {
    Default,
    Metis,
    Random,
    Direct
  };
  const Method defaultMethod=Metis;

  // The graph being placed
  Graph* graph;

  // Dimension of the 2D mesh
  uint32_t width, height;

  // Mapping from node id to partition id
  PartitionId* partitions;

  // Mapping from partition id to subgraph
  Graph* subgraphs;

  // Stores the number of connections between each pair of partitions
  uint64_t** connCount;

  // Mapping from mesh coords to partition id
  PartitionId** mapping;

  // Mapping from partition id to mesh coords
  uint32_t* xCoord;
  uint32_t* yCoord;

  // Cost of current mapping
  uint64_t currentCost;

  // Saved mapping & cost
  PartitionId** mappingSaved;
  uint32_t* xCoordSaved;
  uint32_t* yCoordSaved;
  uint64_t savedCost;

  // Controls which strategy is used
  Method method = Default;

  // Select placer method
  void chooseMethod()
  {
    auto e = getenv("POLITE_PLACER");
    if (e) {
      if (!strcmp(e, "metis"))
        method=Metis;
      else if (!strcmp(e, "random"))
        method=Random;
      else if (!strcmp(e, "direct"))
        method=Direct;
      else if (!strcmp(e, "default") || *e == '\0')
        method=Default;
      else {
        fprintf(stderr, "Don't understand placer method : %s\n", e);
        exit(EXIT_FAILURE);
      }
    }
    if (method == Default)
      method = defaultMethod;
  }

  // Partition the graph using Metis
  void partitionMetis() {
    // Compute total number of edges
    uint32_t numEdges = 0;
    for (uint32_t i = 0; i < graph->incoming->numElems; i++) {
      numEdges += graph->incoming->elems[i]->numElems +
                  graph->outgoing->elems[i]->numElems;
    }

    // Create Metis parameters
    idx_t nvtxs = (idx_t) graph->incoming->numElems;
    idx_t nparts = (idx_t) (width * height);
    idx_t nconn = 1;
    idx_t objval;

    // If there are no vertices
    if (nvtxs == 0) return;

    // If there are more partitions than vertices
    if (nparts >= nvtxs) {
      for (uint32_t i = 0; i < nvtxs; i++)
        partitions[i] = i;
      return;
    }

    // If there is exactly one partition
    if (nparts == 1) {
      for (uint32_t i = 0; i < nvtxs; i++)
        partitions[i] = 0;
      return;
    }

    // Allocate Metis adjacency matrix
    idx_t* xadj = (idx_t*) calloc(nvtxs+1, sizeof(idx_t));
    idx_t* adjncy = (idx_t*) calloc(numEdges, sizeof(idx_t));

    // Populate undirected adjacency matrix
    uint32_t next = 0;
    for (uint32_t i = 0; i < nvtxs; i++) {
      xadj[i] = next;
      Seq<NodeId>* in = graph->incoming->elems[i];
      Seq<NodeId>* out = graph->outgoing->elems[i];
      for (uint32_t j = 0; j < in->numElems; j++)
        adjncy[next++] = (idx_t) in->elems[j];
      for (uint32_t j = 0; j < out->numElems; j++)
        if (! in->member(out->elems[j]))
          adjncy[next++] = (idx_t) out->elems[j];
    }
    xadj[nvtxs] = (idx_t) next;

    // Allocate Metis result array
    idx_t* parts = (idx_t*) calloc(nvtxs, sizeof(idx_t));

    // Specify Metis options
    idx_t options[METIS_NOPTIONS];
    METIS_SetDefaultOptions(options);

    // Invoke partitioner
    // Note: METIS_PartGraphKway gives poor results when the number of
    // vertices is close to the number of partitions, so we use
    // METIS_PartGraphRecursive.
    int ret = METIS_PartGraphRecursive(
      &nvtxs, &nconn, xadj, adjncy,
      NULL, NULL, NULL, &nparts, NULL, NULL, options, &objval, parts);

    // Populate result array
    for (uint32_t i = 0; i < graph->incoming->numElems; i++)
      partitions[i] = (uint32_t) parts[i];

    // Release Metis structures
    free(xadj);
    free(adjncy);
    free(parts);
  }

  // Partition the graph randomly
  void partitionRandom() {
    uint32_t numVertices = graph->incoming->numElems;
    uint32_t numParts = width * height;

    // Populate result array
    srand(0);
    for (uint32_t i = 0; i < numVertices; i++) {
      partitions[i] = rand() % numParts;
    }
  }

  // Partition the graph using direct mapping
  void partitionDirect() {
    uint32_t numVertices = graph->incoming->numElems;
    uint32_t numParts = width * height;
    uint32_t partSize = (numVertices + numParts) / numParts;

    // Populate result array
    for (uint32_t i = 0; i < numVertices; i++) {
      partitions[i] = i / partSize;
    }
  }

  void partition()
  {
    switch(method){
    case Default:
    case Metis:
      partitionMetis();
      break;
    case Random:
      partitionRandom();
      break;
    case Direct:
      partitionDirect();
      break;
    }
  }

  // Create subgraph for each partition
  void computeSubgraphs() {
    uint32_t numPartitions = width*height;

    // Create mapping from node id to subgraph node id
    NodeId* mappedTo = new NodeId [graph->incoming->numElems];

    // Create subgraphs
    for (uint32_t i = 0; i < graph->incoming->numElems; i++) {
      // What parition is this node in?
      PartitionId p = partitions[i];
      // Add node to subgraph
      NodeId n = subgraphs[p].newNode();
      subgraphs[p].setLabel(n, graph->labels->elems[i]);
      mappedTo[i] = n;
    }

    // Add edges to subgraphs
    for (uint32_t i = 0; i < graph->incoming->numElems; i++) {
      PartitionId p = partitions[i];
      Seq<NodeId>* out = graph->outgoing->elems[i];
      for (uint32_t j = 0; j < out->numElems; j++) {
        NodeId neighbour = out->elems[j];
        if (partitions[neighbour] == p)
          subgraphs[p].addEdge(mappedTo[i], mappedTo[neighbour]);
      }
    }

    // Release mapping
    delete [] mappedTo;
  }

  // Computes the number of connections between each pair of partitions
  void computeInterPartitionCounts() {
    uint32_t numPartitions = width*height;

    // Zero the counts
    for (uint32_t i = 0; i < numPartitions; i++)
      for (uint32_t j = 0; j < numPartitions; j++)
        connCount[i][j] = 0;

    // Iterative over graph and count connections
    for (uint32_t i = 0; i < graph->incoming->numElems; i++) {
      Seq<NodeId>* in = graph->incoming->elems[i];
      Seq<NodeId>* out = graph->outgoing->elems[i];
      for (uint32_t j = 0; j < in->numElems; j++)
        connCount[partitions[i]][partitions[in->elems[j]]]++;
      for (uint32_t j = 0; j < out->numElems; j++)
        connCount[partitions[i]][partitions[out->elems[j]]]++;
    }
  }

  // Create random mapping between partitions and mesh
  void randomPlacement() {
    uint32_t numPartitions = width*height;

    // Array of partition ids
    PartitionId pids[numPartitions];
    for (uint32_t i = 0; i < numPartitions; i++) pids[i] = i;

    // Random mapping
    for (uint32_t y = 0; y < height; y++) {
      for (uint32_t x = 0; x < width; x++) {
        int index = rand() % numPartitions;
        PartitionId p = pids[index];
        mapping[y][x] = p;
        xCoord[p] = x;
        yCoord[p] = y;
        numPartitions--;
        for (uint32_t i = index; i < numPartitions; i++) pids[i] = pids[i+1];
      }
    }
  }

  // Save current placement
  void save() {
    savedCost = currentCost;
    for (uint32_t y = 0; y < height; y++)
      for (uint32_t x = 0; x < width; x++)
        mappingSaved[y][x] = mapping[y][x];
    for (uint32_t p = 0; p < width*height; p++) {
      xCoordSaved[p] = xCoord[p];
      yCoordSaved[p] = yCoord[p];
    }
  }

  // Restore old placement
  void restore() {
    currentCost = savedCost;
    for (uint32_t y = 0; y < height; y++)
      for (uint32_t x = 0; x < width; x++)
        mapping[y][x] = mappingSaved[y][x];
    for (uint32_t p = 0; p < width*height; p++) {
      xCoord[p] = xCoordSaved[p];
      yCoord[p] = yCoordSaved[p];
    }
  }

  // Cost function
  // Sum of products of manhatten distance and connection count
  uint64_t cost() {
    uint64_t total = 0;
    uint32_t numPartitions = width*height;
    for (uint32_t i = 0; i < numPartitions; i++) {
      for (uint32_t j = 0; j < i; j++) {
        uint32_t xDist = xCoord[i] >= xCoord[j] ?
                           xCoord[i] - xCoord[j] : xCoord[j] - xCoord[i];
        uint32_t yDist = yCoord[i] >= yCoord[j] ?
                           yCoord[i] - yCoord[j] : yCoord[j] - yCoord[i];
        total += ((uint64_t) (xDist + yDist)) * connCount[i][j];
      }
    }
    return total;
  }

  // Swap two mesh nodes
  inline void swap(uint32_t x, uint32_t y, uint32_t xNew, uint32_t yNew) {
    PartitionId p = mapping[y][x];
    PartitionId pNew = mapping[yNew][xNew];
    mapping[y][x] = pNew;
    mapping[yNew][xNew] = p;
    xCoord[p] = xNew;
    yCoord[p] = yNew;
    xCoord[pNew] = x;
    yCoord[pNew] = y;
  }

  // Swap two mesh nodes only if cost is reduced
  bool trySwap(uint32_t x, uint32_t y, uint32_t xNew, uint32_t yNew) {
    // Try swap
    swap(x, y, xNew, yNew);
    // Undo if cost not lowered
    uint64_t c = cost();
    if (c < currentCost) {
      currentCost = c;
      return true;
    }
    else {
      swap(x, y, xNew, yNew);
      return false;
    }
  }

  // Very simple local search algorithm for placement
  // Repeatedly swap a mesh node with it's neighbour if it lowers cost
  void place(uint32_t numAttempts) {
    // Initialise best cost
    savedCost = ~0;

    for (uint32_t n = 0; n < numAttempts; n++) {
      randomPlacement();
      currentCost = cost();

      bool change;
      do {
        change = false;
        // Loop over mesh
        for (uint32_t y = 0; y < height-1; y++) {
          for (uint32_t x = 0; x < width-1; x++) {
            change = trySwap(x, y, x+1, y) ||
                       trySwap(x, y, x, y+1) ||
                         trySwap(x, y, x+1, y+1) ||
                           change;
          }
        }
      } while (change);

      if (currentCost <= savedCost)
        save();
      else
        restore();
    }
  }

  // Constructor
  Placer(Graph* g, uint32_t w, uint32_t h) {
    graph = g;
    width = w;
    height = h;
    // Allocate the partitions array
    partitions = new PartitionId [g->incoming->numElems];
    // Allocate subgraphs
    subgraphs = new Graph [width*height];
    // Allocate the connection count matrix
    uint32_t numPartitions = width*height;
    connCount = new uint64_t* [numPartitions];
    for (uint32_t i = 0; i < numPartitions; i++)
      connCount[i] = new uint64_t [numPartitions];
    // Allocate mapping from mesh coords to partition id
    mapping = new PartitionId* [h];
    for (uint32_t i = 0; i < h; i++)
      mapping[i] = new PartitionId [w];
    mappingSaved = new PartitionId* [h];
    for (uint32_t i = 0; i < h; i++)
      mappingSaved[i] = new PartitionId [w];
    // Allocate mapping from partition id to mesh coords
    xCoord = new uint32_t [width*height];
    yCoord = new uint32_t [width*height];
    xCoordSaved = new uint32_t [width*height];
    yCoordSaved = new uint32_t [width*height];
    // Pick a placement method, or select default
    chooseMethod();
    // Partition the graph using Metis
    partition();
    // Compute subgraphs, one per partition
    computeSubgraphs();
    // Count connections between each pair of partitions
    computeInterPartitionCounts();
  }

  // Deconstructor
  ~Placer() {
    delete [] partitions;
    delete [] subgraphs;
    uint32_t numPartitions = width*height;
    for (uint32_t i = 0; i < numPartitions; i++) delete [] connCount[i];
    delete [] connCount;
    for (uint32_t i = 0; i < height; i++) delete [] mapping[i];
    delete [] mapping;
    delete [] xCoord;
    delete [] yCoord;
    for (uint32_t i = 0; i < height; i++) delete [] mappingSaved[i];
    delete [] mappingSaved;
    delete [] xCoordSaved;
    delete [] yCoordSaved;
  }
};

#endif
