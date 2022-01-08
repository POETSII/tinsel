// SPDX-License-Identifier: BSD-2-Clause
#ifndef _PLACER_H_
#define _PLACER_H_

#include <vector>
#include <stdexcept>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>

#include <stdint.h>
#include <string.h>
#include <metis.h>
#include <scotch/scotch.h>
#include <POLite/Graph.h>
#include <queue>
#include <omp.h>

typedef uint32_t PartitionId;

// Partition and place a graph on a 2D mesh
struct Placer {
  // Select between different methods
  enum Method {
    Default,
    Metis,
    Random,
    Direct,
    BFS,
	Scotch
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

  // Random numbers
  unsigned int seed;
  void setRand(unsigned int s) { seed = s; };
  int getRand() { return rand_r(&seed); }

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
      else if (!strcmp(e, "bfs"))
        method=BFS;
      else if (!strcmp(e, "scotch"))
        method=Scotch;
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
  void partitionMetis(bool useWeightsIfPresent=true) {
    // Compute total number of edges
    uint32_t numEdges = 0;
    for (uint32_t i = 0; i < graph->numVertices(); i++) {
      numEdges += graph->fanIn(i) + graph->fanOut(i);
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
    idx_t* adjncy = (idx_t*) calloc(numEdges, sizeof(idx_t));

    // Populate undirected adjacency matrix
    uint32_t next = 0;
    for (idx_t i = 0; i < nvtxs; i++) {
      xadj[i] = next;
      Seq<NodeId>* in = graph->incoming()->elems[i];
      Seq<NodeId>* out = graph->outgoing()->elems[i];
      for (uint32_t j = 0; j < in->numElems; j++)
        adjncy[next++] = (idx_t) in->elems[j];
      for (uint32_t j = 0; j < out->numElems; j++)
        if (! in->member(out->elems[j]))
          adjncy[next++] = (idx_t) out->elems[j];
    }
    xadj[nvtxs] = (idx_t) next;

    // Allocate Metis result array
    idx_t* parts = (idx_t*) calloc(nvtxs, sizeof(idx_t));

    idx_t *vwgt=nullptr;
    if(graph->hasWeights() && useWeightsIfPresent){
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
      partitions[i] = (uint32_t) parts[i];

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

  // Partition the graph randomly
  void partitionRandom() {
    uint32_t numVertices = graph->numVertices();
    uint32_t numParts = width * height;

    std::mt19937_64 urng(getRand());
    std::uniform_int_distribution<unsigned> udist(0, numParts-1);
    double scale=std::ldexp(numParts, -64);


    // Populate result array
    for (uint32_t i = 0; i < numVertices; i++) {
      partitions[i] = urng()*scale;
    }
  }

  // Partition the graph using direct mapping
  void partitionDirect() {
    uint32_t numVertices = graph->numVertices();
    uint32_t numParts = width * height;
    uint32_t partSize = (numVertices + numParts) / numParts;

    // Populate result array
    for (uint32_t i = 0; i < numVertices; i++) {
      partitions[i] = i / partSize;
    }
  }

  // Partition the graph using repeated BFS
  void partitionBFS() {
    uint32_t numVertices = graph->numVertices();
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
            Seq<uint32_t>* dests = graph->outgoing()->elems[v];
            for (uint32_t i = 0; i < dests->numElems; i++) {
              uint32_t w = dests->elems[i];
              if (!seen[w]) frontier.push(w);
            }
          }
        }
        while (nextUnseen < numVertices && seen[nextUnseen]) nextUnseen++;
      }

      nextPart++;
    }

    delete [] seen;
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
    case BFS:
      partitionBFS();
      break;
    case Scotch:
      partitionScotch();
      break;
    }
  }

  // Place the graph using Scotch
  void partitionScotch() {
    idx_t nvtxs = (idx_t) graph->incoming()->numElems;
    idx_t nparts = (idx_t) (width * height);

    // If there are no vertices
    if (nvtxs == 0) return;

    // // If there are more partitions than vertices
    // if (nparts >= nvtxs) {
    //   for (uint32_t i = 0; i < nvtxs; i++)
    //     partitions[i] = i;
    //   return;
    // }

    // // If there is exactly one partition
    // if (nparts == 1) {
    //   for (uint32_t i = 0; i < nvtxs; i++)
    //     partitions[i] = 0;
    //   return;
    // }

    SCOTCH_Arch *archptr=(SCOTCH_Arch *)malloc(sizeof(SCOTCH_Arch));
    if(SCOTCH_archInit (archptr)){
      throw std::runtime_error("Couldn't init scotch arch");
    }

    if(SCOTCH_archMesh2(archptr, width, height)){
      throw std::runtime_error("Couldn't create 2d mesh in scotch");
    }

    std::vector<SCOTCH_Num> verttab;
    std::vector<SCOTCH_Num> edgetab;

    for (uint32_t i = 0; i < nvtxs; i++) {
      verttab.push_back(edgetab.size());

      const Seq<NodeId>* in = graph->incoming()->elems[i];
      for (uint32_t j = 0; j < in->numElems; j++){
        edgetab.push_back( in->elems[j] );
      }

      const Seq<NodeId>* out = graph->outgoing()->elems[i];
      for (uint32_t j = 0; j < out->numElems; j++){
        if (! in->member(out->elems[j])){ // TODO: How expensive is this for highly connected graphs?
          edgetab.push_back( out->elems[j] );
        }
      }
    }
    verttab.push_back(edgetab.size());

    SCOTCH_Graph *grafptr=(SCOTCH_Graph *)malloc(sizeof(SCOTCH_Graph));

    if(SCOTCH_graphBuild (grafptr,
      0, //baseval - where do array indices start
      graph->incoming()->numElems, // vertnbr
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

    SCOTCH_Strat *stratptr=(SCOTCH_Strat *)malloc(sizeof(SCOTCH_Strat));
    if(SCOTCH_stratInit(stratptr)){
      throw std::runtime_error("Scotch won't make a strategy.");
    }

    std::vector<SCOTCH_Num> parttab(nvtxs);

    if(SCOTCH_graphMap (grafptr, archptr, stratptr, &parttab[0])){
      throw std::runtime_error("Scotch couldn't map the graph.");
    }

    // Populate result array
    for (uint32_t i = 0; i < graph->incoming()->numElems; i++){
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

    SCOTCH_archExit(archptr);
    free(archptr);
    SCOTCH_graphExit(grafptr);
    free(grafptr);
    SCOTCH_stratInit(stratptr);
    free(stratptr);
  }

  // Create subgraph for each partition
  void computeSubgraphs() {
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
    for (uint32_t i = 0; i < graph->numVertices(); i++) {
      PartitionId p = partitions[i];
      Seq<NodeId>* out = graph->outgoing()->elems[i];
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
    for (uint32_t i = 0; i < graph->numVertices(); i++) {
      Seq<NodeId>* in = graph->incoming()->elems[i];
      Seq<NodeId>* out = graph->outgoing()->elems[i];
      for (uint32_t j = 0; j < in->numElems; j++)
        connCount[partitions[i]][partitions[in->elems[j]]]++;
      for (uint32_t j = 0; j < out->numElems; j++)
        connCount[partitions[i]][partitions[out->elems[j]]]++;
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

    auto trace=[&](int ax,int ay, int bx,int by, uint64_t weight) -> uint64_t
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
    for(int y=0; y<2*height-1; y++){
      for(int x=0; x<2*width-1; x++){
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
  void randomPlacement() {
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
    // Random seed
    setRand(1 + omp_get_thread_num());
    // Allocate the partitions array
    partitions = new PartitionId [g->numVertices()];
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
