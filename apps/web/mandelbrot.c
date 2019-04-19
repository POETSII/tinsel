// Plot the Mandelbrot set

#include <tinsel.h>
#include <grid.h>

int main()
{
  int me = tinselId();

  float x0 = 3.5 * ((float) getX(me) / getWidth()) - 2.5;
  float y0 = 2 * ((float) getY(me) / getHeight()) - 1;

  float x = 0;
  float y = 0;
  int iteration = 0;
  int max_iteration = 32;

  while (x*x + y*y <= 2*2 && iteration < max_iteration) {
    float tmp = x*x - y*y + x0;
    y = 2*x*y + y0;
    x = tmp;
    iteration++;
  }

  emit(iteration*8);
  return 0;
}
