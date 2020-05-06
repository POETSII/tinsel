// SPDX-License-Identifier: BSD-2-Clause
#ifndef _POLITE_H_
#define _POLITE_H_

#include <stdint.h>

#ifdef TINSEL
  #include <tinsel.h>
  #ifdef POLITE_FAST_MAP
    #include <POLite/FastMap/PDevice.h>
  #else
    #include <POLite/PDevice.h>
  #endif
#else
  #ifdef POLITE_FAST_MAP
    #include <POLite/FastMap/PDevice.h>
    #include <POLite/FastMap/PGraph.h>
  #else
    #include <POLite/PDevice.h>
    #include <POLite/PGraph.h>
  #endif
  #include <POLite/Seq.h>
  #include <POLite/Graph.h>
  #include <POLite/Placer.h>
#endif

#endif
