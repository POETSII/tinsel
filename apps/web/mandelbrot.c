// Each thread in a 32x32 grid emits the mandelbrot colour in the range 0..255

#include <tinsel.h>
#include <grid.h>
#include <stdint.h>

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

int main()
{
  // Id for this thread
  int me = tinselId();

  // determine X and Y from thread ID:
  int x=(me >> 5)-15;
  int y=(me & 0x1f)-15;
  emit(mandelbrot_point(x<<24,y<<24,255));

  return 0;
}
