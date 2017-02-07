// Each thread in a 32x32 grid emits a colour in the range 0..255

#include <tinsel.h>
#include <grid.h>

int main()
{
  // Id for this thread
  int me = tinselId();

  // Output the thread id
  // (Divided by 4 so that output lies in range 0..255)
  emit(me >> 2);

  return 0;
}
