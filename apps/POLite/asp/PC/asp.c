#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

// Number of nodes and edges
uint32_t numNodes;
uint32_t numEdges;

// Mapping from node id to array of neighbouring node ids
// First element of each array holds the number of neighbours
uint32_t** neighbours;

// Mapping from node id to bit vector of reaching nodes
uint64_t** reaching;
uint64_t** reachingNext;

// Number of 64-bit words in reaching vector
const uint64_t vectorSize = 7;

void readGraph(const char* filename)
{
  // Read edges
  FILE* fp = fopen(filename, "rt");
  if (fp == NULL) {
    fprintf(stderr, "Can't open '%s'\n", filename);
    exit(EXIT_FAILURE);
  }

  // Create mapping from node id to number of neighbours
  int ret = fscanf(fp, "%d %d", &numNodes, &numEdges);
  uint32_t* count = (uint32_t*) calloc(numNodes, sizeof(uint32_t));
  for (int i = 0; i < numEdges; i++) {
    uint32_t src, dst;
    ret = fscanf(fp, "%d %d", &src, &dst);
    count[src]++;
    count[dst]++;
  }

  // Create mapping from node id to neighbours
  neighbours = (uint32_t**) calloc(numNodes, sizeof(uint32_t*));
  rewind(fp);
  ret = fscanf(fp, "%d %d", &numNodes, &numEdges);
  for (int i = 0; i < numNodes; i++) {
    neighbours[i] = (uint32_t*) calloc(count[i]+1, sizeof(uint32_t));
    neighbours[i][0] = count[i];
  }
  for (int i = 0; i < numEdges; i++) {
    uint32_t src, dst;
    ret = fscanf(fp, "%d %d", &src, &dst);
    neighbours[src][count[src]--] = dst;
    neighbours[dst][count[dst]--] = src;
  }

  // Create mapping from node id to bit vector of reaching nodes
  reaching = (uint64_t**) calloc(numNodes, sizeof(uint64_t*));
  reachingNext = (uint64_t**) calloc(numNodes, sizeof(uint64_t*));
  for (int i = 0; i < numNodes; i++) {
    reaching[i] = (uint64_t*) calloc(vectorSize, sizeof(uint64_t));
    reachingNext[i] = (uint64_t*) calloc(vectorSize, sizeof(uint64_t));
  }

  // Release
  free(count);
  fclose(fp);
}

// Compute sum of all shortest paths
uint64_t ssp(uint32_t from, uint32_t to)
{
  // Sum of distances
  uint64_t sum = 0;

  // Number of nodes in vector
  const int nodesInVector = to-from;

  // Initialise reaching vector for each node
  for (int i = 0; i < numNodes; i++) {
    for (int j = 0; j < vectorSize; j++) {
      reaching[i][j] = 0;
      reachingNext[i][j] = 0;
    }
  }
  for (int i = from; i < to; i++) {
    int j = i-from;
    reaching[i][j/64] = 1ul << (j%64);
    reachingNext[i][j/64] = 1ul << (j%64);
  }

  // Distance increases on each iteration
  uint32_t dist = 1;

  int done = 0;
  while (! done) {
    // For each node
    for (int i = 0; i < numNodes; i++) {
      // For each neighbour
      uint32_t numNeighbours = neighbours[i][0];
      for (int j = 1; j <= numNeighbours; j++) {
        uint32_t n = neighbours[i][j];
        // For each chunk
        for (int k = 0; k < vectorSize; k++)
          reachingNext[i][k] = reachingNext[i][k] | reaching[n][k];
      }
      // Update sums
      for (int k = 0; k < vectorSize; k++) {
        uint64_t diff = reachingNext[i][k] & ~reaching[i][k];
        uint32_t n = __builtin_popcountll(diff);
        sum += n * dist;
      }
    }

    // For each node, update reaching vector
    done = 1;
    for (int i = 0; i < numNodes; i++) {
      uint32_t n = 0;
      for (int k = 0; k < vectorSize; k++) {
        reaching[i][k] = reachingNext[i][k];
        n += __builtin_popcountll(reaching[i][k]);
      }
      if (n != nodesInVector) done = 0;
    }

    dist++;
  }

  return sum;
}

// Cluster nodes that are near to each other
void cluster()
{
  const int nodesPerVector = 64 * vectorSize;

  // Queue of nodes
  int front = 0;
  int back = 0;
  uint32_t* nodes = (uint32_t*) calloc(numNodes, sizeof(uint32_t));

  // Record which nodes have been visited
  uint8_t* visited = calloc(numNodes, sizeof(uint8_t));

  for (int i = 0; i < numNodes; i++) {
    if (visited[i]) continue;

    front = back;
    nodes[back++] = i;
    visited[i] = 1;
    int clusterCount = 1;

    while (front < back && clusterCount < nodesPerVector) {
      uint32_t numNeighbours = neighbours[front][0];
      for (int j = 1; j <= numNeighbours; j++) {
        uint32_t n = neighbours[front][j];
        if (!visited[n]) {
          nodes[back++] = n;
          visited[n] = 1;
          clusterCount++;
          if (clusterCount == nodesPerVector) break;
        }
      }
      front++;
    }

  }

  uint32_t* mapping = (uint32_t*) calloc(numNodes, sizeof(uint32_t));
  for (int i = 0; i < numNodes; i++)
    mapping[nodes[i]] = i;

  // Apply new node ordering to edge list
  uint32_t** newNeighbours = (uint32_t**) calloc(numNodes, sizeof(uint32_t*));
  for (int i = 0; i < numNodes; i++) {
    int j = nodes[i];
    newNeighbours[i] = (uint32_t*)
      calloc(neighbours[j][0]+1, sizeof(uint32_t));
    newNeighbours[i][0] = neighbours[j][0];
    for (int k = 1; k <= neighbours[j][0]; k++)
      newNeighbours[i][k] = mapping[neighbours[j][k]];
  }

  // Use new neighbours array
  neighbours = newNeighbours;

  free(nodes);
}

int main(int argc, char**argv)
{
  if (argc != 2) {
    printf("Specify edges file\n");
    exit(EXIT_FAILURE);
  }
  readGraph(argv[1]);

  //cluster();

  struct timeval start, finish, diff;

  uint64_t sum = 0;
  const int nodesPerVector = 64 * vectorSize;
  gettimeofday(&start, NULL);
  for (int i = 0; i < numNodes; i+= nodesPerVector) {
    int to = (i+nodesPerVector) > numNodes ? numNodes : (i+nodesPerVector);
    sum += ssp(i, to);
    break; /* Only do one iteration, for comparison with POLite version */
  }
  gettimeofday(&finish, NULL);

  printf("Sum of subset of shortest paths = %lu\n", sum);
 
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
