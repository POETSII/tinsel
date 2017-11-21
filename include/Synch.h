#ifndef _SYNCH_H_
#define _SYNCH_H_

#include <stdint.h>

#ifdef TINSEL
  #include <tinsel.h>
  #include <Synch/PDevice.h>
#else
  #include <Synch/PDevice.h>
  #include <POLite/Seq.h>
  #include <POLite/Graph.h>
  #include <POLite/Placer.h>
  #include <Synch/PGraph.h>
#endif

#endif
