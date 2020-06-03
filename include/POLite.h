// SPDX-License-Identifier: BSD-2-Clause
#ifndef _POLITE_H_
#define _POLITE_H_

#include <stdint.h>

// Select default mapper
#if !defined(POLITE_MAP_LOCAL) || \
    !defined(POLITE_MAP_DIST)  || \
    !defined(POLITE_MAP_HYBRID)
  // Default mapper
  #define POLITE_MAP_HYBRID
#endif

#ifdef TINSEL
  #include <tinsel.h>
  #if defined(POLITE_MAP_LOCAL)
    #include <POLite/Local/PDevice.h>
  #elif defined(POLITE_MAP_DIST)
    #include <POLite/Dist/PDevice.h>
  #elif defined(POLITE_MAP_HYBRID)
    #include <POLite/Hybrid/PDevice.h>
  #endif
#else
  #if defined(POLITE_FAST_LOCAL)
    #include <POLite/Local/PDevice.h>
    #include <POLite/Local/PGraph.h>
  #elif defined(POLITE_MAP_DIST)
    #include <POLite/Dist/PDevice.h>
    #include <POLite/Dist/PGraph.h>
  #elif defined (POLITE_MAP_HYBRID)
    #include <POLite/Hybrid/PDevice.h>
    #include <POLite/Hybrid/PGraph.h>
  #endif
  #include <POLite/Seq.h>
  #include <POLite/Graph.h>
  #include <POLite/Placer.h>
#endif

#endif
