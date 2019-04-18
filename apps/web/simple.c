// Each thread in a 2D grid emits its X coordinate

#include <tinsel.h>
#include <grid.h>

int main()
{
  // Id for this thread
  int me = tinselId();

  // Output my X coordinate
  emit(getX(me));

  return 0;
}
