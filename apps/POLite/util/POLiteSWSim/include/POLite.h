#ifndef POLiteSWSim_POLite_h
#define POLiteSWSim_POLite_h

// Everything in here is in namespace POLiteSWSim
#include "POLite/PGraph.h"

// We then explicitly being it out into the global namespace

template <typename DeviceType, typename S, typename E, typename M>
using PGraph = POLiteSWSim::PGraph<DeviceType,S,E,M>;

template <typename S, typename E, typename M>
using PDevice = POLiteSWSim::PDevice<S,E,M>;

using HostLink = POLiteSWSim::HostLink;

template<class Message>
using PMessage = POLiteSWSim::PMessage<Message>;

using PPin = POLiteSWSim::PPin;
using PDeviceId = POLiteSWSim::PDeviceId;
using None = POLiteSWSim::None;

inline constexpr PPin Pin(uint8_t n) { return POLiteSWSim::Pin(n); }

constexpr PPin No = POLiteSWSim::No;
constexpr PPin HostPin = POLiteSWSim::HostPin;

static const unsigned  TinselLogWordsPerMsg = POLiteSWSim::LogWordsPerMsg;
static const unsigned  TinselLogBytesPerMsg = POLiteSWSim::LogBytesPerMsg;
static const unsigned TinselLogBytesPerWord = POLiteSWSim::LogBytesPerWord;
static const unsigned TinselLogBytesPerFlit = POLiteSWSim::LogBytesPerFlit;
static const unsigned TinselCoresPerFPU=POLiteSWSim::CoresPerFPU;
static const unsigned TinselLogBytesPerDRAM=POLiteSWSim::LogBytesPerDRAM;
static const unsigned TinselMeshXBits=POLiteSWSim::MeshXBits;
static const unsigned TinselMeshYBits=POLiteSWSim::MeshYBits;
static const unsigned TinselBoxMeshXLen=POLiteSWSim::BoxMeshXLen;
static const unsigned TinselBoxMeshYLen=POLiteSWSim::BoxMeshYLen;

inline void politeSaveStats(HostLink* hostLink, const char* filename)
{ POLiteSWSim::politeSaveStats(hostLink, filename); }

// This is defined in tinsel-interface.h, and used in some apps.
// Defined empty here for compatibility, but would be nice to get rid of it
#ifndef INLINE
#define INLINE
#endif

#endif