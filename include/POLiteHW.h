

#ifndef POLiteHW_POLite_h
#define POLiteHW_POLite_h

/* This header wraps global namespace things into a struct, so it can be used
   as a template parameter. */

#include "POLite.h"

#include <cstdint>

template<int T_NUM_PINS=POLITE_NUM_PINS>
struct POLiteHW
{
    static constexpr int NUM_PINS = T_NUM_PINS;

    template<int N>
    using rebind_num_pins = POLiteHW<N>;

#ifndef TINSEL
    template <typename DeviceType, typename S, typename E, typename M>
    using PGraph = ::PGraph<DeviceType,S,E,M,T_NUM_PINS>;
#endif

    template <typename S, typename E, typename M>
    using PDevice = ::PDevice<S,E,M>;

#ifndef TINSEL
    using HostLinkParams = ::HostLinkParams;
    using HostLink = ::HostLink;
#endif

    template<class Message>
    using PMessage = ::PMessage<Message>;

    using PPin = ::PPin;
    using None = ::None;

    static inline constexpr PPin Pin(uint8_t n) { return ::Pin(n); }

    static constexpr PPin No = ::No;
    static constexpr PPin HostPin = ::HostPin;

    static const unsigned  LogWordsPerMsg = TinselLogWordsPerMsg;
    static const unsigned  LogBytesPerMsg = TinselLogBytesPerMsg;
    static const unsigned LogBytesPerWord = 2;
    static const unsigned LogBytesPerFlit = TinselLogBytesPerFlit;
    static const unsigned CoresPerFPU=TinselCoresPerFPU;
    static const unsigned LogBytesPerDRAM=TinselLogBytesPerDRAM;
    static const unsigned MeshXBits=TinselMeshXBits;
    static const unsigned MeshYBits=TinselMeshYBits;
    static const unsigned BoxMeshXLen=TinselBoxMeshXLen;
    static const unsigned BoxMeshYLen=TinselBoxMeshYLen;

#ifndef TINSEL
    inline void politeSaveStats(HostLink* hostLink, const char* filename)
    { ::politeSaveStats(hostLink, filename); }
#endif

};

#endif