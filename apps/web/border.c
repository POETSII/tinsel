// Threads in a 32x32 grid emit a border pattern

#include <tinsel.h>
#include <grid.h>

int main()
{
  // Is this thread on the border of the grid?
  int border = north() < 0 || south() < 0 || east() < 0 || west() < 0;

  // Emit a different colour for border threads and interior threads
  emit(border ? 0 : 255);

  return 0;
}
