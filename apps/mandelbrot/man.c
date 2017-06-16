// Mandelbrot set on a LENxLEN grid of threads.
// Each thread handles an 8x8 subgrid of cells.
#define LOG_LEN 5
#define LEN (1 << LOG_LEN)

#include <tinsel.h>

// mandelbrot calculation using 4.28 fixed-point
int mandelbrot_point(int x_0, int y_0, int max_iter) {
 int x = 0, y = 0;
 int iter = 0;

 while(1) {
   int x_sq = (int) ( ( (int64_t)x * (int64_t)x ) >> 28 );
   int y_sq = (int) ( ( (int64_t)y * (int64_t)y ) >> 28 );
   int xy =   (int) ( ( (int64_t)x * (int64_t)y ) >> 28 );

   int two_xy = xy + xy;
   int mag_sq = x_sq + y_sq;

   if (mag_sq > 4 << 28)
     break;
   if (iter >= max_iter)
     break;

   x = x_sq - y_sq + x_0;
   y = two_xy + y_0;

   iter++;
 }
 return iter;
}

// Determine X and Y coords of thread's subgrid
void myXY(int* x, int* y)
{
  int me = tinselId();
  *x = me & (LEN-1);
  *y = me >> LOG_LEN;
}

// Output
// ------

void emitGrid(int (*subgrid)[8])
{
  // Emit the state of the local subgrid
  int xPos, yPos;
  myXY(&xPos, &yPos);
  int x = xPos * 8;
  int y = yPos * 8;
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 8; j++) {
      // Transfer Y coord (12 bits), X coord (12 bits) and colour (8 bits)
      uint32_t coords = ((y+i) << 12) | (x+j);
      uint32_t col = subgrid[i][j] & 0xff;
      tinselHostPut((coords << 8) | col);
    }
}

// Top-level
// ---------

int main()
{
  // Subgrid memory
  // --------------

  int subgrid[8][8];

  // Subgrid location
  // ----------------

  int xPos, yPos;
  myXY(&xPos, &yPos);

  // Suspend threads that are not being used
  if (xPos >= LEN || yPos >= LEN) tinselWaitUntil(TINSEL_CAN_RECV);

  // Compute Mandelbrot subgrid
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) {
      // determine X and Y from thread ID:
      int yy=yPos*8+y;
      int xx=xPos*8+x;
      subgrid[x][y]=255-mandelbrot_point(xx<<21,(yy<<21) + (1<<20),255);
    }

  emitGrid(subgrid);

  return 0;
}
