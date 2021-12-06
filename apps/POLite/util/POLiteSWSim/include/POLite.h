#ifndef POLiteSWSim_POLite_h
#define POLiteSWSim_POLite_h

// Everything in here is in namespace POLiteSWSim
#include "POLite/PGraph.h"

// We then explicitly being it out into the global namespace

using POLiteSWSim::PGraph;
using POLiteSWSim::PDevice;
using POLiteSWSim::HostLink;
using POLiteSWSim::PMessage;

using POLiteSWSim::None;
using POLiteSWSim::No;
using POLiteSWSim::HostPin;
using POLiteSWSim::PPin;
using POLiteSWSim::Pin;
using POLiteSWSim::PDeviceId;

using namespace POLiteSWSim::config;

// This is defined in tinsel-interface.h, and used in some apps.
// Defined empty here for compatibility, but would be nice to get rid of it
#define INLINE

#endif