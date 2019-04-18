// Threads on border emit different colour than interior threads

#include <tinsel.h>
#include <grid.h>

int main()
{
  int me = tinselId();

  // Is this thread on the border?
  int border = north() < 0
            || south() < 0
            || east() < 0
            || west() < 0;

  // Emit a different colour for border threads
  emit(border ? 0 : 255);

  return 0;
}
