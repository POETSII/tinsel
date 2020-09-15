// SPDX-License-Identifier: BSD-2-Clause
// Geometric random graph generator, version 2

#include <vector>
#include <algorithm>
#include <stdio.h>
#include <assert.h>

// Directed or undirected graphs?
int undir = true;

// Count number of edges;
int numEdges = 0;

// Add edge
void addEdge(int src, int dst)
{
  printf("%d %d\n", src, dst); numEdges++;
  if (undir) { printf("%d %d\n", dst, src); numEdges++; }
}

int main(int argc, char *argv[]) 
{
  if (argc != 7) {
    printf("Usage: GenGeoGraph2 <d|u> <width> <height> "
                               "<dist> <fanout> <seed>\n");
    return -1;
  }

  // Directed or undirected?
  if (argv[1][0] == 'd') undir = false;

  // X and Y lengths of geometric space
  int xLen = atoi(argv[2]);
  int yLen = atoi(argv[3]);

  // Number of vertices
  // (Every cell in geometric space contains a vertex)
  int numVertices = xLen * yLen;

  // Distance threshold for connectivity
  // A value of n means a manhatten distance of 2n
  // All neighbours of a vertex must be within distance of that vertex
  int dist = atoi(argv[4]);

  // Vertex fanout
  int fanout = atoi(argv[5]);
  if (undir) fanout /= 2;

  // Random seed
  int seed = atoi(argv[6]);
  srand(seed);

  // Create 2D array for geometric space
  int** space = new int* [yLen];
  for (int y = 0; y < yLen; y++)
    space[y] = new int [xLen];

  // Initialise space
  int v = 0;
  for (int y = 0; y < yLen; y++)
    for (int x = 0; x < xLen; x++)
      space[y][x] = v++;

  // For each vertex, pick neighbours
  // We don't avoid duplicate edges here
  for (int y = 0; y < yLen; y++) {
    for (int x = 0; x < xLen; x++) {
      int src = space[y][x];
      int count = 0;
      // Nearest neighbour edges to ensure graph is connected
      if (x > 0) { addEdge(src, space[y][x-1]); count++; }
      if (y > 0) { addEdge(src, space[y-1][x]); count++; }
      // Pick neighbours, within distance, at random
      int top = std::max(0, y-dist);
      int bottom = std::min(yLen-1, y+dist);
      int left = std::max(0, x-dist);
      int right = std::min(xLen-1, x+dist);
      while (count < fanout) {
        int rx = left + rand() % (right-left);
        int ry = top + rand() % (bottom-top);
        addEdge(src, space[ry][rx]);
        count++;
      }
    }
  }

  fprintf(stderr, "Seed: %u\n", seed);
  fprintf(stderr, "Vertices: %u\n", numVertices);
  fprintf(stderr, "Edges: %d\n", numEdges);

  return 0;
}
