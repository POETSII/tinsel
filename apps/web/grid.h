#ifndef _GRID_H_
#define _GRID_H_

// Direction
typedef enum {N, S, E, W} Dir;

// Opposite direction
Dir opposite(Dir d);

// Get X coord of thread in grid
int getX(int threadId);

// Get Y coord of thread in grid
int getY(int threadId);

// Determine thread id from X and Y coords
int fromXY(int x, int y);

// Neighbouring thread in given direction
// (Return -1 if no neighbour, i.e. at edge of grid)
int north();
int south();
int east();
int west();

// Emit colour for executing thread
void emit(int col);

#endif
