// SPDX-License-Identifier: BSD-2-Clause
// Geometric random graph generator

#include <vector>
#include <algorithm>
#include <stdio.h>

// Probality of adding an edge to a vertex that's within distance
double Chance;

// Number of neighbours chosen completely at random for a vertex
const double RandConns = 0;

// Directed or undirected?
bool undir = true;

bool chance()
{
  double x = ((double) rand()) / RAND_MAX;
  return x <= Chance;
}

int main(int argc, char *argv[]) 
{
  if (argc != 8) {
    printf("Usage: GenGeoGraph <d|u> <width> <height> <vertices> "
                              "<dist> <chance> <seed>\n");
    return -1;
  }

  // Directed or undirected?
  if (argv[1][0] == 'd') undir = false;

  // X and Y lengths of geometric space
  int xLen = atoi(argv[2]);
  int yLen = atoi(argv[3]);

  // Number of vertices
  int numVertices = atoi(argv[4]);

  // Distance threshold for connectivity
  // A value of n means a manhatten distance of 2n
  int dist = atoi(argv[5]);

  // Probality of adding an edge to a vertex that's within distance
  Chance = atof(argv[6]);

  // Random seed
  int seed = atoi(argv[7]);
  srand(seed);

  int numEdges = 0;
  // Create 2D array for geometric space
  int** space = new int* [yLen];
  for (int y = 0; y < yLen; y++)
    space[y] = new int [xLen];

  // Initialise space
  for (int y = 0; y < yLen; y++)
    for (int x = 0; x < xLen; x++)
      space[y][x] = -1;

  // Randomly insert vertices into space
  for (int v = 0; v < numVertices; ) {
    int x = rand() % xLen;
    int y = rand() % yLen;
    if (space[y][x] < 0) {
      space[y][x] = v;
      v++;
    }
  }

  // For each vertex, find a default neighbour
  // (This will be used to ensure graphs are fully connected)
  int* def = new int [numVertices];
  int last = -1;
  for (int y = 0; y < yLen; y++) {
    for (int x = 0; x < xLen; x++) {
      int v = space[y][x];
      if (v >= 0) {
        if (last >= 0) {
          def[v] = last;
          def[last] = v;
        }
        last = v;
      }
    }
  }

  // Some completely random connections
  for (int y = 0; y < yLen; y++) {
    for (int x = 0; x < xLen; x++) {
      int src = space[y][x];
      if (src >= 0) {
        for (int i = 0; i < RandConns; i++) {
          double chance = ((double) rand()) / RAND_MAX;
          double d = chance * (numVertices-1);
          int dst = (int) d;
          if (src != dst) {
            printf("%d %d\n", src, dst); numEdges++;
            if (undir) { printf("%d %d\n", dst, src); numEdges++; }
          }
        }
      }
    }
  }

  // Insert connections
  for (int y = 0; y < yLen; y++) {
    for (int x = 0; x < xLen; x++) {
      int src = space[y][x];
      if (src >= 0) {
        int top = std::max(0, y-dist);
        int bottom = std::min(yLen-1, y+dist);
        int left = std::max(0, x-dist);
        int right = std::min(xLen-1, x+dist);
        bool connected = false;
        for (int b = top; b <= bottom; b++) {
          for (int a = left; a <= right; a++) {
            int dst = space[b][a];
            if (chance()) {
              if (dst >= 0 && src != dst) {
                printf("%d %d\n", src, dst); numEdges++;
                if (undir) { printf("%d %d\n", dst, src); numEdges++; }
                if (b < y) connected = true;
              }
            }
          }
        }
        if (!connected) {
          printf("%d %d\n", src, def[src]); numEdges++;
          if (undir) { printf("%d %d\n", def[src], src); numEdges++; }
        }
      }
    }
  }

  fprintf(stderr, "Seed: %u\n", seed);
  fprintf(stderr, "Vertices: %u\n", numVertices);
  fprintf(stderr, "Edges: %d\n", numEdges);
  fprintf(stderr, "Average fanout: %lf\n",
    (double) numEdges / (double) numVertices);
  fprintf(stderr, "===\n");

  return 0;
}
