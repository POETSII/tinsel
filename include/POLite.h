#ifndef _POLITE_H_
#define _POLITE_H_

#include <stdint.h>

#ifdef TINSEL
  #include <tinsel.h>
  #include <POLite/PDevice.h>
#else
  #include <POLite/PDevice.h>
  #include <POLite/Seq.h>
  #include <POLite/Graph.h>
  #include <POLite/Placer.h>
  #include <POLite/PGraph.h>
#endif

#endif
