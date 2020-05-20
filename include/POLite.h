// SPDX-License-Identifier: BSD-2-Clause
#ifndef _POLITE_H_
#define _POLITE_H_

#include <stdint.h>

#ifdef TINSEL
  #include <tinsel.h>
  #if defined(POLITE_USE_PR)
    #include <POLite/PR/PDevice.h>
  #elif defined(POLITE_USE_PR_FAST)
    #include <POLite/PRFast/PDevice.h>
  #else
    #include <POLite/LocalMcast/PDevice.h>
  #endif
#else
  #if defined(POLITE_PR)
    #include <POLite/PR/PDevice.h>
    #include <POLite/PR/PGraph.h>
  #elif defined(POLITE_PR_FAST)
    #include <POLite/PRFast/PDevice.h>
    #include <POLite/PRFast/PGraph.h>
  #else
    #include <POLite/LocalMcast/PDevice.h>
    #include <POLite/LocalMcast/PGraph.h>
  #endif
  #include <POLite/Seq.h>
  #include <POLite/Graph.h>
  #include <POLite/Placer.h>
#endif

#endif
