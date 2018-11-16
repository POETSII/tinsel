// Geometric random graph generator

#include <vector>
#include <algorithm>
#include <stdio.h>

// X and Y lengths of geometric space
const int XLen = 3000;
const int YLen = 3000;

// Number of vertices
const int NumVertices = 600000;

// Distance threshold for connectivity
// A value of n means a manhatten distance of 2n
const int Dist = 14;

// Probality of adding an edge to a vertex that's within distance
const double Chance = 1.0;

bool chance()
{
  double x = ((double) rand()) / RAND_MAX;
  return x <= Chance;
}

// Edges
struct Edge { int src, dst; };

int main()
{
  // Create 2D array for geometric space
  int** space = new int* [YLen];
  for (int y = 0; y < YLen; y++)
    space[y] = new int [XLen];

  // Edge list
  std::vector<Edge> edges;

  // Initialise space
  for (int y = 0; y < XLen; y++)
    for (int x = 0; x < XLen; x++)
      space[y][x] = -1;

  // Randomly insert vertices into space
  for (int v = 0; v < NumVertices; ) {
    int x = rand() % XLen;
    int y = rand() % YLen;
    if (space[y][x] < 0) {
      space[y][x] = v;
      v++;
    }
  }

  // For each vertex, find a default neighbour
  // (This will be used to ensure graphs are fully connected)
  int* def = new int [NumVertices];
  int last = -1;
  for (int y = 0; y < YLen; y++) {
    for (int x = 0; x < XLen; x++) {
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

  // Insert connections
  for (int y = 0; y < YLen; y++) {
    for (int x = 0; x < XLen; x++) {
      int src = space[y][x];
      if (src >= 0) {
        int top = std::max(0, y-Dist);
        int bottom = std::min(YLen-1, y+Dist);
        int left = std::max(0, x-Dist);
        int right = std::min(XLen-1, x+Dist);
        bool connected = false;
        for (int b = top; b <= bottom; b++) {
          for (int a = left; a <= right; a++) {
            int dst = space[b][a];
            if (chance()) {
              if (dst >= 0 && src != dst) {
                Edge edge = { src, dst };
                edges.push_back(edge);
                if (b < y) connected = true;
              }
            }
          }
        }
        if (!connected) {
          Edge edge1 = { src, def[src] };
          Edge edge2 = { def[src], src };
          edges.push_back(edge1);
          edges.push_back(edge2);
        }
      }
    }
  }

  fprintf(stderr, "Vertices: %u\n", NumVertices);
  fprintf(stderr, "Edges: %lu\n", edges.size());

  for (int i = 0; i < edges.size(); i++) {
    Edge e = edges[i];
    printf("%d %d\n", e.src, e.dst);
  }

  return 0;
}
