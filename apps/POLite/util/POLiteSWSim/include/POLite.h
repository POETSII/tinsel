#ifndef POLiteSWSim_POLite_h
#define POLiteSWSim_POLite_h

#ifndef POLITE_NUM_PINS
#define POLITE_NUM_PINS 1
#endif

// Everything in here is in namespace POLiteSWSim
#include "POLite/PGraph.h"

// We then explicitly being it out into the global namespace

template <typename DeviceType, typename S, typename E, typename M>
using PGraph = POLiteSWSim<POLITE_NUM_PINS>::PGraph<DeviceType,S,E,M>;

template <typename S, typename E, typename M>
using PDevice = POLiteSWSim<POLITE_NUM_PINS>::PDevice<S,E,M>;

using HostLink = POLiteSWSim<POLITE_NUM_PINS>::HostLink;

template<class Message>
using PMessage = POLiteSWSim<POLITE_NUM_PINS>::PMessage<Message>;

using PPin = POLiteSWSim<POLITE_NUM_PINS>::PPin;
using PDeviceId = POLiteSWSim<POLITE_NUM_PINS>::PDeviceId;
using None = POLiteSWSim<POLITE_NUM_PINS>::None;

inline constexpr PPin Pin(uint8_t n) { return POLiteSWSim<POLITE_NUM_PINS>::Pin(n); }

constexpr PPin No = POLiteSWSim<POLITE_NUM_PINS>::No;
constexpr PPin HostPin = POLiteSWSim<POLITE_NUM_PINS>::HostPin;

static const unsigned  TinselLogWordsPerMsg = POLiteSWSim<POLITE_NUM_PINS>::LogWordsPerMsg;
static const unsigned  TinselLogBytesPerMsg = POLiteSWSim<POLITE_NUM_PINS>::LogBytesPerMsg;
static const unsigned TinselLogBytesPerWord = POLiteSWSim<POLITE_NUM_PINS>::LogBytesPerWord;
static const unsigned TinselLogBytesPerFlit = POLiteSWSim<POLITE_NUM_PINS>::LogBytesPerFlit;
static const unsigned TinselCoresPerFPU=POLiteSWSim<POLITE_NUM_PINS>::CoresPerFPU;
static const unsigned TinselLogBytesPerDRAM=POLiteSWSim<POLITE_NUM_PINS>::LogBytesPerDRAM;
static const unsigned TinselMeshXBits=POLiteSWSim<POLITE_NUM_PINS>::MeshXBits;
static const unsigned TinselMeshYBits=POLiteSWSim<POLITE_NUM_PINS>::MeshYBits;
static const unsigned TinselBoxMeshXLen=POLiteSWSim<POLITE_NUM_PINS>::BoxMeshXLen;
static const unsigned TinselBoxMeshYLen=POLiteSWSim<POLITE_NUM_PINS>::BoxMeshYLen;

inline void politeSaveStats(HostLink* hostLink, const char* filename)
{ POLiteSWSim<POLITE_NUM_PINS>::politeSaveStats(hostLink, filename); }

// This is defined in tinsel-interface.h, and used in some apps.
// Defined empty here for compatibility, but would be nice to get rid of it
#ifndef INLINE
#define INLINE
#endif

#endif