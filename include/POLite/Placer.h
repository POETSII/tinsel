// SPDX-License-Identifier: BSD-2-Clause
#ifndef _PLACER_H_
#define _PLACER_H_

#include <stdint.h>
#include <metis.h>
#include <POLite/Graph.h>
#include <queue>
#include <omp.h>
#include <random>
#include <algorithm>

/* I (dt10) couldnt get MTMetis to work on more than 1 thread without crashing */
#ifdef HAVE_MT_METIS
#include <mtmetis.h>
#endif

#define POLITE_NOINLINE __attribute__((noinline))

typedef uint32_t PartitionId;

enum PlacerMethod {
    Default,
    Metis,
    Random,
    RandomBalanced,
    Direct,
    BFS,
    BFS_then_Metis, // Perform BFS at the board level, then switch to metis
    MTMetis
  };

inline const char *placer_method_to_string(PlacerMethod m)
{
  switch(m){
  case Default: return "Default";
  case Metis: return "Metis";
  case Random: return "Random";
  case RandomBalanced: return "RandomBalanced";
  case Direct: return "Direct";
  case BFS: return "BFS";
  case BFS_then_Metis: return "BFS_then_Metis";
  case MTMetis: return "MTMetis";
  default:
    assert(0);
    return "UNKNOWN";
  }
}

inline PlacerMethod parse_placer_method(::std::string method){
  std::transform(method.begin(), method.end(), method.begin(), [](char c){ return std::tolower(c); });

  if(method=="metis"){
      return PlacerMethod::Metis;
  }else if(method=="bfs"){
      return PlacerMethod::BFS;
  }else if(method=="random"){
      return PlacerMethod::Random;
  }else if(method=="randombalanced"){
      return PlacerMethod::RandomBalanced;
  }else if(method=="direct"){
      return PlacerMethod::Direct;
  }else if(method=="default"){
      return PlacerMethod::Default;
  }else if(method=="mtmetis"){
      return PlacerMethod::MTMetis;
  }else if(method=="bfs_then_metis"){
      return PlacerMethod::BFS_then_Metis;
  }else{
      fprintf(stderr, "Unknown placement method '%s'\n", method.c_str());
      exit(1);
  }
}

// Partition and place a graph on a 2D mesh
template<class TEdgeWeight=None>
struct Placer {
  // Select between different methods
  
  #ifdef HAVE_MT_METIS
  const PlacerMethod defaultMethod=MTMetis;  
  #else
  const PlacerMethod defaultMethod=Metis;
  #endif

  // The graph being placed
  Graph<TEdgeWeight>* graph;

  // Dimension of the 2D mesh
  uint32_t width, height;

  // Mapping from node id to partition id
  PartitionId* partitions;

  // Mapping from partition id to subgraph
  Graph<None>* subgraphs;

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

  int recursion_level=0; // May be used to intelligently select methods at system vs board vs FPGA levels

  // Random numbers
  unsigned int seed;
  void setRand(unsigned int s) { seed = s; };
  int getRand() { return rand_r(&seed); }

  // Controls which strategy is used
  PlacerMethod method = Default;

  // will be set to the method actually used
  PlacerMethod method_chosen;

  // Select placer method
  void chooseMethod()
  {
    auto e = getenv("POLITE_PLACER");
    if (e) {
      if (!strcmp(e, "metis"))
        method=Metis;
      else if (!strcmp(e, "mtmetis"))
        method=MTMetis;
      else if (!strcmp(e, "random"))
        method=Random;
      else if (!strcmp(e, "direct"))
        method=Direct;
      else if (!strcmp(e, "bfs"))
        method=BFS;
      else if (!strcmp(e, "bfs_then_metis"))
        method=BFS_then_Metis;
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

  // Partition the graph using Metis or MetisMT
  POLITE_NOINLINE void partitionMetis() {
    // TODO : use 64-bit metis
    if(graph->getEdgeCount() >= (1u<<31)){
      fprintf(stderr, "This graph has at least 2^31 edges, and will fail in 32-bit metis.\n");
      exit(1);
    }

    // Compute total number of edges
    uint32_t numEdges = 0;
    for (uint32_t i = 0; i < graph->nodeCount(); i++) {
      numEdges += graph->fanIn(i) +
                  graph->fanOut(i);
    }

    // Create Metis parameters
    idx_t nvtxs = (idx_t) graph->nodeCount();
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
     unsigned nIn=graph->fanIn(i);
     graph->exportIncomingNodeIds(i, nIn, adjncy+next);
     next += nIn;

     unsigned nOut=graph->fanOut(i);
     graph->exportOutgoingNodeIds(i, nOut, adjncy+next);
     next += nOut;
    }
    xadj[nvtxs] = (idx_t) next;

    // Allocate Metis result array
    idx_t* parts = (idx_t*) calloc(nvtxs, sizeof(idx_t));

    // Invoke partitioner
    // Note: METIS_PartGraphKway gives poor results when the number of
    // vertices is close to the number of partitions, so we use
    // METIS_PartGraphRecursive.
    int ret;
    if(method==MTMetis && (recursion_level==0) && method!=Metis){
      method_chosen=MTMetis;
      #ifndef HAVE_MT_METIS
      fprintf(stderr, "MTMetis is selected, but does not seem to have been compiled in.");
      exit(1);
      #else
      fprintf(stderr, "Using MTMetis : warning, this is very unstable.\n");
      static_assert(sizeof(idx_t)==sizeof(mtmetis_vtx_type));
      static_assert(sizeof(idx_t)==sizeof(mtmetis_adj_type));
      static_assert(sizeof(idx_t)==sizeof(mtmetis_pid_type));
      double options[MTMETIS_NOPTIONS];
      for(unsigned i=0; i<MTMETIS_NOPTIONS; i++){
        options[i]= MTMETIS_VAL_OFF;
      }
      options[MTMETIS_OPTION_NTHREADS]=2;
      mtmetis_wgt_type objval;
      ret = MTMETIS_PartGraphRecursive(
        (mtmetis_vtx_type*)&nvtxs, (mtmetis_vtx_type*)&nconn, (mtmetis_adj_type*)xadj, (mtmetis_vtx_type*)adjncy,
        NULL, NULL, NULL, (mtmetis_pid_type*)&nparts, NULL, NULL, options, &objval, (mtmetis_pid_type*)parts);
      #endif
    }else{
      method_chosen=Metis;

      // Specify Metis options
      idx_t options[METIS_NOPTIONS];
      METIS_SetDefaultOptions(options);
      ret = METIS_PartGraphRecursive(
        &nvtxs, &nconn, xadj, adjncy,
        NULL, NULL, NULL, &nparts, NULL, NULL, options, &objval, parts);
    }

    // Populate result array
    for (uint32_t i = 0; i < graph->nodeCount(); i++)
      partitions[i] = (uint32_t) parts[i];

    // Release Metis structures
    free(xadj);
    free(adjncy);
    free(parts);
  }

  // Partition the graph randomly
  void partitionRandom() {
    method_chosen=Random;

    uint32_t numVertices = graph->nodeCount();
    uint32_t numParts = width * height;

    // Populate result array
    for (uint32_t i = 0; i < numVertices; i++) {
      partitions[i] = getRand() % numParts;
    }
  }

  // Partition the graph randomly, but keeping the same number of devices per thread
  void partitionRandomBalanced() {
    method_chosen=RandomBalanced;

    uint32_t numVertices = graph->nodeCount();
    uint32_t numParts = width * height;

    std::mt19937_64 rng(getRand());

    std::vector<PartitionId> dests(numParts);
    std::iota(dests.begin(), dests.end(), 0);

    // We allocate in rounds, handing one device out to each partition per round.
    // This takes O(numVertices) RNG calls, which are slow, but not too bad for 64-bit
    // mersenne twister. Note: 32-bit MT is generally a lot slower than 64-bit
    uint32_t done=0;
    while(done < numVertices){
      std::shuffle(dests.begin(), dests.end(), rng);

      uint32_t todo=std::min<size_t>(dests.size(), numVertices-done);
      for(unsigned i=0; i<todo; i++){
        partitions[i+done] = dests[i];
      }
      done+=todo;
    }
  }

  // Partition the graph using direct mapping
  void partitionDirect() {
    method_chosen=Direct;

    uint32_t numVertices = graph->nodeCount();
    uint32_t numParts = width * height;
    uint32_t partSize = (numVertices + numParts) / numParts;

    // Populate result array
    for (uint32_t i = 0; i < numVertices; i++) {
      partitions[i] = i / partSize;
    }
  }

  // Partition the graph using repeated BFS
  void partitionBFS() {
    method_chosen=BFS;

    uint32_t numVertices = graph->nodeCount();
    uint32_t numParts = width * height;
    uint32_t partSize = (numVertices + numParts) / numParts;

    // Visited bit for each vertex
    bool* seen = new bool [numVertices];
    memset(seen, 0, numVertices);

    // Next vertex to visit
    uint32_t nextUnseen = 0;

    // Next partition id
    uint32_t nextPart = 0;

    while (nextUnseen < numVertices) {
      // Frontier
      std::queue<uint32_t> frontier;
      uint32_t count = 0;

      while (nextUnseen < numVertices && count < partSize) {
        // Sized-bounded BFS from nextUnseen
        frontier.push(nextUnseen);
        while (count < partSize && !frontier.empty()) {
          uint32_t v = frontier.front();
          frontier.pop();
          if (!seen[v]) {
            seen[v] = true;
            partitions[v] = nextPart;
            count++;
            // Add unvisited neighbours of v to the frontier
            /*Seq<uint32_t>* dests = graph->outgoing->elems[v];
            for (uint32_t i = 0; i < dests->numElems; i++) {
              uint32_t w = dests->elems[i];
              if (!seen[w]) frontier.push(w);
            }*/
            graph->walkOutgoingNodeIds(v, [&](uint32_t id){
              if(!seen[id]){
                frontier.push(id);
              }
            });
          }
        }
        while (nextUnseen < numVertices && seen[nextUnseen]) nextUnseen++;
      }

      nextPart++;
    }

    delete [] seen;
  }

  PlacerMethod choose_default()
  {
    if(graph->getEdgeCount() >= (1u<<30) || graph->nodes.size() >= (1u<<28) ){
      return BFS;
    }else{
      return Metis;
    }
  }

  void partition()
  {
    PlacerMethod method_now=method;
    if(method==Default){
      method_now=choose_default();
    }

    switch(method){
    case Default:
      fprintf(stderr, "Expecting explicit method by now\n");
      exit(1);
    case MTMetis:
    case Metis:
      if(graph->getEdgeCount() >= (1u<<30)){
        fprintf(stderr, "Warning: Metis chosen as placement method, but graph has at least 2^31 edges. Falling back on BFS.\n");
        partitionBFS();
        break;
      }
      partitionMetis();
      break;
    case Random:
      partitionRandom();
      break;
    case Direct:
      partitionDirect();
      break;
    case BFS:
      partitionBFS();
      break;
    case BFS_then_Metis:
      if(recursion_level==0 || (graph->getEdgeCount() >= (1u<<30))){
        partitionBFS();
      }else{
        partitionMetis();
      }
      break;
    }
  }

  // Create subgraph for each partition
  POLITE_NOINLINE void computeSubgraphs() {
    uint32_t numPartitions = width*height;

    // Create mapping from node id to subgraph node id
    NodeId* mappedTo = new NodeId [graph->nodeCount()];

    // Create subgraphs
    for (uint32_t i = 0; i < graph->nodeCount(); i++) {
      // What parition is this node in?
      PartitionId p = partitions[i];
      // Add node to subgraph
      NodeId n = subgraphs[p].newNode();
      subgraphs[p].setLabel(n, graph->getLabel(i));
      mappedTo[i] = n;
    }

    // Add edges to subgraphs
    for (uint32_t i = 0; i < graph->nodeCount(); i++) {
      PartitionId p = partitions[i];
      /*Seq<NodeId>* out = graph->outgoing->elems[i];
      for (uint32_t j = 0; j < out->numElems; j++) {
        NodeId neighbour = out->elems[j];
        if (partitions[neighbour] == p)
          subgraphs[p].addEdge(mappedTo[i], mappedTo[neighbour]);
      }*/
      graph->walkOutgoingNodeIds(i, [&](uint32_t neighbour){
        if (partitions[neighbour] == p)
          subgraphs[p].addEdge(mappedTo[i], mappedTo[neighbour]);
      });
    }

    // Release mapping
    delete [] mappedTo;
  }

  // Computes the number of connections between each pair of partitions
  POLITE_NOINLINE  void computeInterPartitionCounts() {
    uint32_t numPartitions = width*height;

    // Zero the counts
    for (uint32_t i = 0; i < numPartitions; i++)
      for (uint32_t j = 0; j < numPartitions; j++)
        connCount[i][j] = 0;

    // Iterative over graph and count connections
    for (uint32_t i = 0; i < graph->nodeCount(); i++) {
     graph->walkIncomingNodeIds(i, [&](uint32_t j){
        connCount[partitions[i]][partitions[j]]++;
      });
      graph->walkOutgoingNodeIds(i, [&](uint32_t j){
        connCount[partitions[i]][partitions[j]]++;
      });
    }
  }

  // Create random mapping between partitions and mesh
  POLITE_NOINLINE void randomPlacement() {
    uint32_t numPartitions = width*height;

    // Array of partition ids
    PartitionId pids[numPartitions];
    for (uint32_t i = 0; i < numPartitions; i++) pids[i] = i;

    // Random mapping
    for (uint32_t y = 0; y < height; y++) {
      for (uint32_t x = 0; x < width; x++) {
        int index = getRand() % numPartitions;
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
  POLITE_NOINLINE Placer(Graph<TEdgeWeight>* g, uint32_t w, uint32_t h, int _recursion_level, PlacerMethod _method=PlacerMethod::Default) {
    recursion_level = _recursion_level;
    graph = g;
    width = w;
    height = h;
    method=_method;
    // Random seed
    setRand(1 + omp_get_thread_num());
    // Allocate the partitions array
    partitions = new PartitionId [g->nodeCount()];
    // Allocate subgraphs
    subgraphs = new Graph<None> [width*height];
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
  POLITE_NOINLINE ~Placer() {
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
