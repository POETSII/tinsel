// SPDX-License-Identifier: BSD-2-Clause
#ifndef _PLACER_H_
#define _PLACER_H_

#include <vector>
#include <stdexcept>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#include <thread>
#include <mutex>

#include <stdint.h>
#include <string.h>
#include <metis.h>
#include <scotch/scotch.h>
#include <POLite/Graph.h>
#include <POLite/Noise.h>
#include <queue>
#include <omp.h>
#include <unordered_set>
#include <random>
#include <algorithm>
#include "ParallelFor.h"

#define POLITE_NOINLINE __attribute__((noinline))

typedef uint32_t PartitionId;

enum PlacerMethod {
    Default,
    Metis,
    Random,
    Permutation,
    Direct,
    Dealer,
    BFS,
	  Scotch,
    BFS_then_Metis // Perform BFS at the board level, then switch to metis
  };

inline const char *placer_method_to_string(PlacerMethod m)
{
  switch(m){
  case Default: return "default";
  case Metis: return "metis";
  case Random: return "random";
  case Permutation: return "permutation";
  case Direct: return "direct";
  case BFS: return "bfs";
  case Scotch: return "scotch";
  case BFS_then_Metis: return "bfs_then_metis";
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
  }else if(method=="permutation"){
      return PlacerMethod::Permutation;
  }else if(method=="direct"){
      return PlacerMethod::Direct;
  }else if(method=="scotch"){
    return PlacerMethod::Scotch;
  }else if(method=="default"){
      return PlacerMethod::Default;
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
    ParallelFlag parallel_flag=ParallelFlag::Default;

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
      method = parse_placer_method(e);
    }
    if (method == Default)
      method = defaultMethod;
  }

  // Partition the graph using Metis
  void partitionMetis(bool useWeightsIfPresent=true) {
    // TODO : use 64-bit metis
    if(graph->getEdgeCount() >= (1u<<31)){
      fprintf(stderr, "This graph has at least 2^31 edges, and will fail in 32-bit metis.\n");
      exit(1);
    }

    // Create Metis parameters
    idx_t nvtxs = (idx_t) graph->numVertices();

    idx_t nparts = (idx_t) (width * height);
    idx_t nconn = 1;
    idx_t objval;

    // If there are no vertices
    if (nvtxs == 0) return;

    // If there are more partitions than vertices
    if (nparts >= nvtxs) {
      for (idx_t i = 0; i < nvtxs; i++)
        partitions[i] = i;
      return;
    }

    // If there is exactly one partition
    if (nparts == 1) {
      for (idx_t i = 0; i < nvtxs; i++)
        partitions[i] = 0;
      return;
    }

    // Allocate Metis adjacency matrix
    idx_t* xadj = (idx_t*) calloc(nvtxs+1, sizeof(idx_t));
    // Compute total number of edges and populate xadx
    uint32_t numEdges = 0;
    for (uint32_t i = 0; i < graph->numVertices(); i++) {
      xadj[i] = numEdges;
      numEdges += graph->fanIn(i) + graph->fanOut(i);
    }
    xadj[nvtxs] = numEdges;

    idx_t* adjncy = (idx_t*) calloc(numEdges, sizeof(idx_t));

    // Populate undirected adjacency matrix
    uint32_t next = 0;
    parallel_for_with_grain<unsigned>(0, nvtxs, 256, [&](unsigned i){
      unsigned base=xadj[i];
      unsigned nIn=graph->fanIn(i);
      graph->exportIncomingNodeIds(i, nIn, adjncy+base);
      base += nIn;

      unsigned nOut=graph->fanOut(i);
      graph->exportOutgoingNodeIds(i, nOut, adjncy+base);
    });

    // Allocate Metis result array
    idx_t* parts = (idx_t*) calloc(nvtxs, sizeof(idx_t));

    idx_t *vwgt=nullptr;
    if(graph->hasWeights() && useWeightsIfPresent){
      fprintf(stderr, "Using weights\n");
      vwgt=(idx_t*)calloc(nvtxs, sizeof(idx_t));
      unsigned minw=10000000, maxw=0;
      for(int i=0; i<nvtxs; i++){
        vwgt[i] = graph->getWeight(i);
        minw=std::min<unsigned>(minw, vwgt[i]);
        maxw=std::max<unsigned>(maxw, vwgt[i]);
      }
    }

    // Specify Metis options
    idx_t options[METIS_NOPTIONS];
    METIS_SetDefaultOptions(options);

    // Invoke partitioner
    // Note: METIS_PartGraphKway gives poor results when the number of
    // vertices is close to the number of partitions, so we use
    // METIS_PartGraphRecursive.
    int ret = METIS_PartGraphRecursive(
      &nvtxs, &nconn, xadj, adjncy,
      vwgt, NULL, NULL, &nparts, NULL, NULL, options, &objval, parts);

    // Populate result array
    for (uint32_t i = 0; i < graph->numVertices(); i++)
    for (uint32_t i = 0; i < graph->nodeCount(); i++){
      partitions[i] = (uint32_t) parts[i];
    }

    // Release Metis structures
    free(xadj);
    free(adjncy);
    free(parts);
    free(vwgt);

  //   // Perform random placement now, so that others can do their
  //   // own placement
  // #ifdef SCOTCH
  //   randomPlacement();
  // #endif
    currentCost = cost();
  }

  // Partition the graph randomly. Note that this results in unequal loads
  void partitionRandom() {
    uint32_t numVertices = graph->numVertices();
    method_chosen=Random;

    uint32_t numParts = width * height;

    std::mt19937_64 urng(getRand());
    double scale=std::ldexp(numParts, -64);

    // Populate result array
    for (uint32_t i = 0; i < numVertices; i++) {
      partitions[i] = urng()*scale;
    }
  }

  // Partition the graph randomly. This should result in equal loads
  void partitionPermutation() {
    partitionDealer();

    std::mt19937_64 urng(getRand());

    uint32_t numVertices = graph->nodeCount();
    std::shuffle(partitions, partitions+numVertices, urng);

    method_chosen=Permutation;
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

  // Partition the graph by dealing from deck
  void partitionDealer() {
    uint32_t numVertices = graph->nodeCount();
    uint32_t numParts = width * height;
    
    // Populate result array
    for (uint32_t i = 0; i < numVertices; i++) {
      partitions[i] = i % numParts;
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
    default:
      fprintf(stderr, "Invalid placement enum value %d\n", method);
      exit(1);
    case Default:
      fprintf(stderr, "Expecting explicit method by now\n");
      exit(1);
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
    case Dealer:
      partitionDealer();
      break;
    case Permutation:
      partitionPermutation();
      break;
    case Scotch:
      partitionScotch();
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

  // Place the graph using Scotch
  void partitionScotch() {
    // I dislike static mutexes as much as they next person, but
    // there is a problem with libscotch6 for Ubuntu, which does
    // not appear to be thread safe. There is a parser in there
    // somewhere which is not re-entrant.
    static std::mutex scotch_mutex;

    idx_t nvtxs = (idx_t) graph->nodeCount();
    idx_t nparts = (idx_t) (width * height);

    // If there are no vertices
    if (nvtxs == 0) return;

    std::vector<SCOTCH_Num> verttab;
    std::vector<SCOTCH_Num> edgetab;

    // TODO : robin hood
    std::unordered_set<unsigned> seen;

    // TODO  : weights
    for (uint32_t i = 0; i < (uint32_t)nvtxs; i++) {
      seen.clear();
      verttab.push_back(edgetab.size());

      graph->walkIncomingNodeIds(i, [&](unsigned j){
        seen.insert(i);
      });

      graph->walkOutgoingNodeIds(i, [&](unsigned j){
        seen.insert(j);
      });

      for(auto v : seen){
        if(v!=i){
         edgetab.push_back(v);
        }
      }
      assert(seen.size()!=0);
    }
    assert(seen.size()!=0);
    verttab.push_back(edgetab.size());

    std::vector<SCOTCH_Num> parttab(nvtxs);

    ///////////////////////////////////////////////////
    // Enter scotch lock
    std::unique_lock<std::mutex> scotch_lock(scotch_mutex);

    SCOTCH_Arch *archptr=SCOTCH_archAlloc();
    if(SCOTCH_archInit (archptr)){
      throw std::runtime_error("Couldn't init scotch arch");
    }

    if(SCOTCH_archMesh2(archptr, width, height)){
      throw std::runtime_error("Couldn't create 2d mesh in scotch");
    }

    SCOTCH_Graph *grafptr=SCOTCH_graphAlloc();

    if(SCOTCH_graphBuild (grafptr,
      0, //baseval - where do array indices start
      graph->nodeCount(),
      &verttab[0],
      &verttab[1], //vendtab. Settings to verttab+1 means it is a compact edge array
      0, // velotab, Integer load per vertex. Not used here.
      0, // vlbltab, vertex label tab (?)
      edgetab.size(), // edgenbr,
      &edgetab[0],
      0 // edlotab, load on each arc
    )){
      throw std::runtime_error("Scotch didn't want to build a graph.");
    }

    if(SCOTCH_graphCheck (grafptr)){
      throw std::runtime_error("Scotch does not like the graph we built. Is it consistent?");
    }

    SCOTCH_Strat *stratptr=SCOTCH_stratAlloc();
    if(SCOTCH_stratInit(stratptr)){
      throw std::runtime_error("Scotch won't make a strategy.");
    }

    if(SCOTCH_graphMap (grafptr, archptr, stratptr, &parttab[0])){
      throw std::runtime_error("Scotch couldn't map the graph.");
    }

    scotch_lock.unlock();
    // Exit scotch lock
    //////////////////////////////////////////

    // Populate result array
    for (uint32_t i = 0; i < graph->nodeCount(); i++){
      partitions[i] = (uint32_t) parttab[i];
    }

    // Explicitly set up placement
    for (uint32_t y = 0; y < height; y++) {
      for (uint32_t x = 0; x < width; x++) {
        unsigned p=y*width+x;
        mapping[y][x] = p;
        xCoord[p] = x;
        yCoord[p] = y;
      }
    }

    currentCost = cost();

    ////////////////////////////////////
    // Enter scotch lock
    scotch_lock.lock();

    SCOTCH_archExit(archptr);
    SCOTCH_memFree(archptr);

    SCOTCH_graphExit(grafptr);
    SCOTCH_memFree(grafptr);

    SCOTCH_stratInit(stratptr);
    SCOTCH_memFree(stratptr);

    scotch_lock.unlock();
    // Exit scotch lock
    ///////////////////////////////////////
  }

  // Create subgraph for each partition
  POLITE_NOINLINE void computeSubgraphs() {
    uint32_t numPartitions = width*height;

    // Create mapping from node id to subgraph node id
    NodeId* mappedTo = new NodeId [graph->numVertices()];

    // Create subgraphs
    bool hasWeights=graph->hasWeights();
    for (uint32_t i = 0; i < graph->numVertices(); i++) {
      // What parition is this node in?
      PartitionId p = partitions[i];
      // Add node to subgraph
      NodeId n = subgraphs[p].newNode();
      subgraphs[p].setLabel(n, graph->getLabel(i));
      if(hasWeights){
        subgraphs[p].setWeight(n, graph->getWeight(i));
      }
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

  // If we are on a width*height mesh, then calculate the
  // number of channels passing through the intermediate
  // manhattan routers
  std::vector<uint64_t> computeInterLinkCounts()
  {
    /*
      Take each * as a node (e.g. board/FPGA), and
      the lines are the links.

          x
          0  1  2  3
      y 0 *--*--*--*   0
          |  |  |  |
        1 *--*--*--*   2
          |  |  |  |
        2 *--*--*--*   4
          |  |  |  |
        3 *--*--*--*   6
                       7
          0  2  4  6 7

      Being lazy, the link between
        (x,y) and (x+1,y) is at  (x+x+1),y*2) = (x*2+1,y*2)
      and the link between
        (x,y) and (x,y+1) is at  (x,y+y+1) = (x*2,y*2+1)
    */

    std::vector<uint64_t> counts((2*width-1)*(2*height-1), 0);

    auto traverse=[&](int x, int y, int dx, int dy, uint64_t weight)
    {
      int vx=2*x+dx;
      int vy=2*y+dy;
      int index=vy*(2*width-1) + vx;
      //fprintf(stderr, " (%u,%u)+(%d,%d) -> %u\n", x,y,dx,dy,index);
      counts.at( index ) += weight;
    };

    auto trace=[&](int ax,int ay, int bx,int by, uint64_t weight)
    {
      /* From Network.bsv:
          function Route route(NetAddr addr);
                  if (addr.board.y < b.y) return Down;
              else if (addr.board.y > b.y) return Up;
              else if (addr.host.valid) return addr.host.value == 0 ? Left : Right;
              else if (addr.board.x < b.x) return Left;
              else if (addr.board.x > b.x) return Right;

          So route in y first, then x
      */

     int x=ax, y=ay;

     // Record the route taken through links
     int hops=0;
      while(y!=by){
        int dir=y<by ? +1 : -1;
        traverse(x, y, 0, dir, weight);
        y += dir;
        hops++;
      }
      while(x!=bx){
        int dir=x<bx ? +1 : -1;
        traverse(x, y, dir, 0, weight);
        x += dir;
        hops++;
      }

      // Self-connections should be removed by now.
      assert(hops>0);

      // Record the cost of routes begining and ending in the node
      traverse(ax,ay, 0, 0, hops*weight);
      traverse(bx,by, 0, 0, hops*weight);

      /*if(ax==0 && bx==0){
        fprintf(stderr, "a=(0,0), hops=%u, weight=%llu, counts[0,0]=%llu\n", hops, weight, counts[0]);
      }*/
    };

    // computeInterPartitionCounts();

    for(int p1=0; p1+1 < width*height; p1++){
      for(int p2=p1+1; p2 < width*height; p2++){
        trace(
          xCoord[p1], yCoord[p1],
          xCoord[p2], yCoord[p2],
          connCount[p1][p2]
        );
      }
    }

    fprintf(stderr, "Per-link route cost: count of total routes traversing this link:\n");
    double sumSquareLinkWeight=0;
    double sumLinkWeight=0;
    double maxLinkWeight=0;
    double numLinks=0;
    for(int y=0; y<2*height-1; y++){
      for(int x=0; x<2*width-1; x++){
        if((x&1)&&(y&1)){
          fprintf(stderr, " %6s", " ");
        }else if( (x&1) || (y&1) ){
          auto w=counts[y*(2*width-1)+x];
          fprintf(stderr, " %6llu", (unsigned long long)w);
          sumSquareLinkWeight += pow((double)w,2);
          sumLinkWeight += w;
          maxLinkWeight = std::max(maxLinkWeight, (double)w);
          numLinks++;
        }else{
          fprintf(stderr, " %6s", "+");
        }
      }
      fprintf(stderr, "\n");
    }

    fprintf(stderr, "Per-node route cost: sum(distance*count) for routes beginning and ending in each node:\n");
    double sumSquareNodeWeight=0;
    double sumNodeWeight=0;
    double maxNodeWeight=0;
    double numNodes=0;
    for(unsigned y=0; y<2*height-1; y++){
      for(unsigned x=0; x<2*width-1; x++){
        if((x&1)&&(y&1)){
          fprintf(stderr, " %6s", " ");
        }else if( ! ((x&1) || (y&1)) ){
          auto w=counts[y*(2*width-1)+x];
          fprintf(stderr, " %6llu", w);
          sumSquareNodeWeight += pow((double)w,2);
          sumNodeWeight += w;
          maxNodeWeight = std::max(maxNodeWeight, (double)w);
          numNodes++;
        }else if(x&1){
          fprintf(stderr, " %6s", "-");
        }else{
          fprintf(stderr, " %6s", "|");
        }
      }
      fprintf(stderr, "\n");
    }

    double meanLinkWeight=sumLinkWeight/numLinks;
    double stddevLinkWeight=sqrt( sumSquareLinkWeight/numLinks - meanLinkWeight*meanLinkWeight  );
    fprintf(stderr, "Per-link: mean=%.1f, stddev=%.1f, max=%.1f\n",
      meanLinkWeight, stddevLinkWeight, maxLinkWeight
    );

    double meanNodeWeight=sumNodeWeight/numLinks;
    double stddevNodeWeight=sqrt( sumSquareNodeWeight/numNodes - meanNodeWeight*meanNodeWeight  );
    fprintf(stderr, "Per-node: mean=%.1f, stddev=%.1f, max=%.1f\n",
      meanNodeWeight, stddevNodeWeight, maxNodeWeight
    );


    return counts;
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
      for (uint32_t x = 0; x < width; x++) {
        mapping[y][x] = mappingSaved[y][x];
      }
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
    if (method == Scotch)
      return;
    // Initialise best cost
    savedCost = ~0;

    // If we want no effort at all we want to avoid local refinement
    if(numAttempts==0){
      randomPlacement();
      currentCost = cost();
      save();
    }else{
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
