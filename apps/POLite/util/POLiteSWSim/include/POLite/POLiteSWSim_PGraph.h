#ifndef POLiteSWSim_PGraph_h
#define POLiteSWSim_PGraph_h

#include <cstdint>
#include <memory>
#include <vector>
#include <cassert>
#include <array>
#include <deque>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <random>
#include <atomic>
#include <algorithm>
#include <unistd.h>

#ifndef POLITE_NUM_PINS
#define POLITE_NUM_PINS 1
#endif

template<int T_NUM_PINS=POLITE_NUM_PINS>
struct POLiteSWSim {

    template<int N>
    using rebind_num_pins = POLiteSWSim<N>;

static inline bool get_option_bool(const char *name, bool def) 
{
    auto *p=getenv(name);
    if(p){
        std::string s(p);
        std::transform(s.begin(), s.end(), s.begin(), [](char c){ return std::tolower(c);});
        if(s=="1" || s=="yes" || s=="true"){
            fprintf(stderr, "%s=%s, flag=true\n", name, s.c_str());
            return true;
        }
        if(s=="0" || s=="no" || s=="false"){
            fprintf(stderr, "%s=%s, flag=false\n", name, s.c_str());
            return false;
        }
        fprintf(stderr, "POLiteSWSim::PGraph::PGraph() : Error - didn't understand value for env var %s=%s\n", name, p);
        exit(1);
    }
    return def;
}   

static inline unsigned get_option_unsigned(const char *name, unsigned def)
{
    auto *p=getenv(name);
    if(p){
        try{
            return std::stoul(p);
        }catch(...){
            fprintf(stderr, "POLiteSWSim::PGraph::PGraph() : Error - didn't understand value for env var %s=%s\n", name, p);
            exit(1);
        }
    }
    return def;
}   

using PDeviceId = uint32_t;
typedef int32_t PinId;

// This is a static limit on the number of pins per device
#ifndef POLITE_NUM_PINS
#define POLITE_NUM_PINS 1
#endif

// Pins
//   No      - means 'not ready to send'
//   HostPin - means 'send to host'
//   Pin(n)  - means 'send to application pin number n'
struct PPin{
  uint8_t index;

  constexpr bool operator==(PPin o) const { return index==o.index; }
  constexpr bool operator!=(PPin o) const { return index!=o.index; }
};
static_assert(sizeof(PPin)==1, "Expecting PPin to be 1 byte.");

static constexpr PPin No = PPin{0};
static constexpr PPin HostPin = PPin{1};

static constexpr PPin Pin(uint8_t n) { return PPin{(uint8_t)(n+2)}; }

// TODO: This is getting unmanageable and they are inconsistent
static const unsigned  LogWordsPerMsg = 4;
static const unsigned  LogBytesPerMsg = 6;
static const unsigned LogBytesPerWord = 2;
static const unsigned LogBytesPerFlit = 4;
static const unsigned CoresPerFPU=32;
static const unsigned LogThreadsPerCore=1;
static const unsigned ThreadsPerCore  =1<<LogThreadsPerCore;
static const unsigned LogBytesPerDRAM=27;
static const unsigned MeshXBits=3;
static const unsigned MeshYBits=3;
static const unsigned BoxMeshXLen=4;
static const unsigned BoxMeshYLen=4;
static const unsigned LogCoresPerDCache=4;
static const unsigned LogThreadsPerDCache=LogCoresPerDCache<<LogThreadsPerCore;
static const unsigned LogThreadsPerBoard=7;
static const unsigned ThreadsPerBoard=1<<LogThreadsPerBoard;
static const unsigned CoresPerBoard = ThreadsPerBoard / ThreadsPerCore;



enum PlacerMethod
{ Default };

static inline PlacerMethod parse_placer_method(const std::string &)
{ return Default; }

static inline std::string placer_method_to_string(PlacerMethod p)
{
    assert(p==false);
    return "Default";
}

// For template arguments that are not used
struct None {};

// Implementation detail
class PGraphBase
{
public:
    virtual ~PGraphBase()
    {}

    virtual PGraphBase *clone() const=0;

    virtual void sim_prepare() =0;

    virtual bool sim_step(
        std::mt19937_64 &rng,
        std::function<void (void *, size_t)> send_cb,
        std::function<bool (uint32_t &, size_t &, void *)>  recv_cb
    ) =0;
};

// HostLink parameters
struct HostLinkParams {
  uint32_t numBoxesX = 1;
  uint32_t numBoxesY = 1;
  bool useExtraSendSlot = false;

  // Used to indicate when the hostlink moves through different phases, especially in construction
  std::function<void(const char *)> on_phase;

    // Used to allow retries when connecting to the socket. When performing rapid sweeps,
  // it is quite common for the first attempt in the next process to fail.
  int max_connection_attempts = 1;
};

template<class M>
struct PMessage
{
    M payload;
};

class HostLink
{
public:
    using Impl = POLiteSWSim<T_NUM_PINS>;

    static uint32_t toAddr(uint32_t meshX, uint32_t meshY,
             uint32_t coreId, uint32_t threadId)
    {
        throw std::runtime_error("Not implemented for sim.");
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cond;

    std::deque<std::vector<uint8_t>> m_dev2host;
    std::deque<std::pair<uint32_t,std::vector<uint8_t>>> m_host2dev; // queue of (dest,payload) pairs

    std::atomic<bool> m_user_waiting;
    std::atomic<bool> m_worker_interrupt;
    std::atomic<bool> m_worker_running;

    unsigned verbosity;

    void worker_proc()
    {
        // To make debugging easier we hold the lock and occasionally release
        // it, so that effectively only one thread is running
        std::unique_lock<std::mutex> lk(m_mutex);

        std::mt19937_64 rng;
        uint32_t seed= time(0)+(uintptr_t)(malloc) +(uintptr_t)(toAddr)+(uintptr_t)(&rng);
        //fprintf(stderr, "POLiteSim : seed = %u\n", seed);
        rng.seed(seed);

        std::function<void (void *,size_t)> send_cb=[&](void *p, size_t n)
        {
            // We don't put anything else in the PMessage.
            static_assert(sizeof(PMessage<int>) == sizeof(int));

            // Need to expand up to full message size when sending to host.
            // Tinsel guarantees that padding will be zero: `toPCIe.put(0);`
            std::vector<uint8_t> tmp(1<<LogBytesPerMsg, 0);
            memcpy(&tmp[0], p, n);

            assert(lk.owns_lock());
            m_dev2host.push_back(std::move(tmp));
        };

        std::function<bool (uint32_t &, size_t &, void *)> recv_cb=[&](uint32_t &dst, size_t &size, void * buffer)
        {
            // We don't put anything else in the PMessage.
            static_assert(sizeof(PMessage<int>) == sizeof(int));

            assert(lk.owns_lock());
            if(m_host2dev.empty()){
                return false;
            }

            auto msg=std::move(m_host2dev.front());
            m_host2dev.pop_front();

            dst=msg.first;
            size=msg.second.size();
            memset(buffer, 0xCC, 1<<LogBytesPerMsg);
            memcpy(buffer, &msg.second[0], size);

            return true;
        };

        m_graph->sim_prepare();

        while(1){
            if(!m_graph->sim_step(rng, send_cb, recv_cb)){
                if(verbosity>=2){
                    fprintf(stderr, "POLiteSWSim::HostLink : Info - Exiting device worker thread due to devices finishing. verbosity=%d\n", verbosity);
                }
                break;
            }

            if(m_worker_interrupt.load()){
                if(verbosity>=2){
                    fprintf(stderr, "POLiteSWSim::HostLink : Info - Exiting worker thread due to interrupt (host finishing while devices still running, probably fine).\n");
                }
                break;
            }
            if( m_user_waiting.load() ){
                m_cond.notify_all();

                lk.unlock();
                std::this_thread::yield();
                lk.lock();
            }
        }

        m_cond.notify_all();

        m_worker_running.store(false);
    }

    std::thread m_worker;
public:
    HostLink()
        : verbosity(POLiteSWSim::get_option_unsigned("POLITE_SW_SIM_VERBOSITY", 1))
    {}

    HostLink(const HostLinkParams)
        : verbosity(POLiteSWSim::get_option_unsigned("POLITE_SW_SIM_VERBOSITY", 1))
    {
    }

    ~HostLink()
    {
        m_worker_interrupt.store(true);
        if(m_worker.joinable()){
            m_worker.join();
        }
        if(m_graph){
            delete m_graph;
            m_graph=0;
        }
    }

    // Implementation detail
    // This hostlink owns the graph, and should delete it
    PGraphBase *m_graph=0;

    void attach_graph(PGraphBase *graph)
    {
        assert(m_graph==0);
        m_graph=graph;
    }

    // No-op for SW
    void boot(const char */*code*/, const char */*data*/)
    {};

    // Needs to create an _independent_ thread which runs in the 
    // background, then return
    void go()
    {
        assert(m_graph);
        m_worker_interrupt.store(false);
        m_worker_running.store(true);
        m_worker=std::thread([=](){ worker_proc(); });
    }
    /*
      // Send a message (blocking by default)
    bool send(uint32_t dest, uint32_t numFlits, void* msg, bool block = true)
    {
        std::unique_lock<std::mutex> lk(m_mutex);

        m_cond.wait(lk, [&](){
            return (m_host2dev.size()<16) || !m_worker_running.load();
        });

        if(!m_worker_running.load()){
            fprintf(stderr, "POLiteSWSim::HostLink::recvMsg : Error - HostLink::send was called, but devices have all finished - message will be lost.");
            exit(1);
        }

        std::vector<uint8_t> payload;
        payload.assign( (uint8_t*)msg, (numFlits<<LogBytesPerFlit)+(uint8_t*)msg
        m_host2dev.push_back({dest, std::move(payload)};
    }
    */

    // Blocking receive of max size message
    void recvMsg(void* msg, uint32_t numBytes)
    {
        assert(m_graph);

        if(numBytes > (1<<LogBytesPerMsg)){
            fprintf(stderr, "POLiteSWSim::HostLink::recvMsg : Error - Attempt to read more than max sized message.");
            exit(1);
        }

        while(1){
            /* This threading is _incredibly_ bad. It is inefficient,
            probably relies on undefined behaviour, and is almost surely
            not correct. */

            m_user_waiting.store(true);

            std::unique_lock<std::mutex> lk(m_mutex);

            m_user_waiting.store(false);
            m_cond.notify_all();

            if(m_dev2host.empty()){
                if(!m_worker_running.load()){
                    fprintf(stderr, "POLiteSWSim::HostLink::recvMsg : Error - HostLink::recvMsg was called, but devices have all finished and there are no pending messages - app will block.");
                    exit(1);
                }else{
                    lk.unlock();
                    std::this_thread::yield();
                    usleep(1000);
                    continue; // Release lock, then reaquires
                }
            }

            auto front=m_dev2host.front();
            m_dev2host.pop_front();

            const int MaxMessageSize=1<<LogBytesPerMsg;

            if(MaxMessageSize!=front.size()){
                fprintf(stderr, "POLiteSWSim::HostLink::recvMsg : Error - (internal POLiteSWSim error) host received non max sized message from device : expected = %u, got=%u.\n",
                    MaxMessageSize, (unsigned)front.size()
                );
                exit(1);
            }
            memcpy(msg, &front[0], numBytes);

            return;
        }
    }

    // Blocking receive of max size message
    void recv(void* msg)
    {
        assert(m_graph);
        recvMsg(msg, 1<<LogBytesPerMsg);
    }

    bool canRecv()
    {
        assert(m_graph);

        m_user_waiting.store(true);

        std::unique_lock<std::mutex> lk(m_mutex);

        m_user_waiting.store(false);
        m_cond.notify_all();

        return !m_dev2host.empty();
    }

    bool isSim() const
    { return true; }

    bool hasTerminated()
    {
        m_user_waiting.store(true);

        std::unique_lock<std::mutex> lk(m_mutex);

        m_user_waiting.store(false);
        m_cond.notify_all();

        return m_dev2host.empty() && !m_worker_running.load();
    }

    void recvBulkNonBlock(
         int bufferSize,
         int *bufferValidBytes, // Number of valid bytes in the buffer, both before and after call
         void *buffer
    ) {
        if(bufferSize % (1<<LogBytesPerMsg)){
            throw std::runtime_error("Buffer is not multiple of message size.");
        }
        if( (bufferSize - *bufferValidBytes) % (1<<LogBytesPerMsg)){
            throw std::runtime_error("Space left is not a multiple of message size.");
        }
        if( (bufferSize - *bufferValidBytes) == 0){
            throw std::runtime_error("No space left in buffer.");
        }
        
        if(canRecv()){
            recv( *bufferValidBytes + (char*)buffer );
            *bufferValidBytes += 1<<LogBytesPerMsg;
        }
    }

    void recvBulkNonBlock(
         int bufferSize,
         int *bufferValidBytes, // Number of valid bytes in the buffer, both before and after call
         void *buffer,
         std::function<void(void *)> on_message
    ){

        int buffer_valid=*bufferValidBytes;
        recvBulkNonBlock(bufferSize, &buffer_valid, buffer);
        int off=0;
        while(off + (1<<LogBytesPerMsg) <= buffer_valid){
            on_message( off + (char*)buffer );
            off += (1<<LogBytesPerMsg);
        }
        buffer_valid -= off;
        if(buffer_valid){
            memmove(buffer,  off+(char*)buffer, buffer_valid);
        }
        *bufferValidBytes=buffer_valid;
    }

    bool pollStdOut(FILE* /*outFile*/)
    {
        assert(m_graph);
        return false;
    }

    void setStdOutFilterProc(std::function<bool(uint32_t,const char *)> filter)
    {
        assert(m_graph);
    }
};

// No-op for SW
inline static void politeSaveStats(HostLink* /*hostLink*/, const char* /*filename*/)
{}

template <typename T, typename = void>
struct PState_has_device_performance_counters
{
  static constexpr bool value = false;
  static constexpr unsigned count = 0;
};

template <typename T>
struct PState_has_device_performance_counters<T, decltype((void)T::device_performance_counters, void())>
{
  static constexpr unsigned count_helper()
  {
    constexpr int res= sizeof(T::device_performance_counters) / sizeof(uint32_t);
    return res;
  }

  static constexpr unsigned count = count_helper();
  static constexpr bool value = count > 0;
};

template <typename S, typename E, typename M>
struct PDevice {
    static constexpr bool HasDevicePerfCounters = PState_has_device_performance_counters<S>::value;
    static constexpr unsigned NumDevicePerfCounters = PState_has_device_performance_counters<S>::count;

    // Impementation
    PPin _realReadyToSend = No;

  // State
  S* s;
  PPin* readyToSend;
  uint32_t numVertices;
  uint16_t time;

  // Handlers
  void init();
  void send(volatile M* msg);
  void recv(M* msg, E* edge);
  bool step();
  bool finish(volatile M* msg);
};

template <typename TDeviceType,
          typename S, typename E, typename M,
          int TT_NUM_PINS=T_NUM_PINS,
          bool TT_ENABLE_CORE_PERF_COUNTERS=false,
          bool TT_ENABLE_THREAD_PERF_COUNTERS=false
          >
struct PThread {
    static_assert(TT_NUM_PINS==T_NUM_PINS); // Well this was stupid.

    using DeviceType = TDeviceType;
     using Impl = POLiteSWSim<T_NUM_PINS>;

    static constexpr bool ENABLE_CORE_PERF_COUNTERS = TT_ENABLE_CORE_PERF_COUNTERS;
    static constexpr bool ENABLE_THREAD_PERF_COUNTERS = TT_ENABLE_THREAD_PERF_COUNTERS;

    enum ThreadPerfCounters {
    SendHandlerCalls,
    TotalSendHandlerTime,
    BlockedSends,
    MsgsSent,   
    
    MsgsRecv,
    TotalRecvHandlerTime,

    MinBarrierActive,
    SumBarrierActiveDiv256,
    MaxBarrierActive,

    Fake_BeginBarrier, // Not an actual perf counter, just used to keep track of things
    Fake_LastActive, // Not an actual perf counter, just used to keep track of things

    Fake_ThreadPerfCounterSize
  };
};

template <typename DeviceType, typename S, typename E, typename M>
class PGraph
    : public PGraphBase // Implementation detail
{
public:
    static constexpr int NUM_PINS = T_NUM_PINS;
private:
    // This structure must be directly exposed to clients
    struct PState
    {
        // Implementation stuff
        // For outgoing we keep that 0==empty and 1==host
        std::array<std::vector<std::pair<PDeviceId,unsigned>>,NUM_PINS+2> outgoing;
        std::vector<E> incoming;
        unsigned fanOut=0;

        // User visible stuff
        S state;

        std::mutex lock;
    };

    HostLink *m_hostlink=0;

    std::mutex m_lock;
    uint32_t m_maxFanOut=0;
    uint32_t m_maxFanIn=0;
    uint64_t m_edgeCount=0;

    unsigned verbosity=0;
public:
    static constexpr bool is_simulation = true;

    std::function<void(const char *message)> on_fatal_error;
    std::function<void(const char *part)> on_phase_hook;
    std::function<void(const char *key, double value)> on_export_value;
    std::function<void(const char *key, const char *value)> on_export_string;

    PlacerMethod placer_method=Default;

    bool mapVerticesToDRAM=false; // Dummy flag
    bool mapInEdgeHeadersToDRAM=false; // Dummy flag
    bool mapInEdgeRestToDRAM=false; // Dummy flag
    bool mapOutEdgesToDRAM=false; // Dummy flag

    // uint32_t i = graph.numDevices;
    uint32_t numDevices = 0;

    // S &s = graph.devices[i]->state;
    std::vector<std::shared_ptr<PState>> devices;

    std::vector<DeviceType> device_states;

    virtual PGraphBase *clone() const
    {
        if(is_running_copy){
            throw std::runtime_error("Can't clone running copy.");
        }

        PGraph *res=new PGraph();
        res->is_running_copy=true;
        res->on_fatal_error=on_fatal_error;
        res->on_phase_hook=on_phase_hook;
        res->on_export_value=on_export_value;
        res->on_export_string=on_export_string;
        res->m_maxFanIn=m_maxFanIn;
        res->m_maxFanOut=m_maxFanOut;
        res->devices=devices; // copy
        res->numDevices=numDevices;
        res->verbosity=verbosity;
        return res;
    }

    PGraph()
    {
        deliver_out_of_order=POLiteSWSim::get_option_bool("POLITE_SW_SIM_DELIVER_OUT_OF_ORDER", true);
        verbosity=POLiteSWSim::get_option_unsigned("POLITE_SW_SIM_VERBOSITY", 1);
    }

    PGraph(int /*x*/, int /*y*/)
    {
        PGraph();
    }

    ~PGraph()
    {
    }

    PDeviceId newDevice()
    {
        PDeviceId id=numDevices++;
        devices.push_back(std::make_shared<PState>());
        memset(&devices.back()->state, 0, sizeof(S));
        return id;
    }

    PDeviceId newDevices(unsigned n)
    {
        PDeviceId id=numDevices;
        for(unsigned i=0; i<n; i++){
            newDevice();
        }
        return id;
    }

    uint32_t getDeviceId(uint32_t threadId, unsigned deviceOffset)
    {
        throw std::runtime_error("Not implemented for sim.");
    }

    int getMeshLenX() const
    { return 1; }

    int getMeshLenY() const
    { return 1; }

    void setDeviceWeight(unsigned id, unsigned weight)
    {
        assert(weight>0);
    }

    void reserveOutgoingEdgeSpace(PDeviceId from, PinId pin, unsigned n)
    {
        auto &d=devices.at(from)->outgoing[pin];
        d.reserve(d.size()+n);
    }

    unsigned getThreadIdFromDeviceId(PDeviceId id) const
    { return 0; }

    unsigned numThreads() const
    { return 1; }


    void addEdge(PDeviceId from, PinId pin, PDeviceId to)
    {
        addLabelledEdge({}, from, pin, to);
    }

    void addLabelledEdgeImpl(E edge, PDeviceId from, PinId pin, PDeviceId to, bool lock_dst)
    {
        assert(pin<NUM_PINS);
        std::shared_ptr<PState> dstDev=devices.at(to);
        std::shared_ptr<PState> srcDev=devices.at(from);
        unsigned key=dstDev->incoming.size();      
        {
            std::unique_lock<std::mutex> lk(dstDev->lock, std::defer_lock);
            if(lock_dst){
                lk.lock();
            }
            dstDev->incoming.push_back(edge);
        }
        srcDev->outgoing[pin].push_back({to,key});  
        srcDev->fanOut++;

        {
            std::unique_lock<std::mutex> lk(m_lock, std::defer_lock);
            if(lock_dst){
                lk.lock();
            }
            m_maxFanOut=std::max<uint32_t>(m_maxFanOut, srcDev->fanOut);
            m_maxFanIn=std::max<uint32_t>(m_maxFanIn, dstDev->incoming.size());
            ++m_edgeCount;
        }
    }

    void addLabelledEdge(E edge, PDeviceId from, PinId pin, PDeviceId to)
    {
        addLabelledEdgeImpl(edge,from, pin, to, false);
    }

    void addLabelledEdgeLockedDst(E edge, PDeviceId from, PinId pin, PDeviceId to)
    {
        addLabelledEdgeImpl(edge,from, pin, to, true);
    }

    void addEdgeLockedDst(PDeviceId from, PinId pin, PDeviceId to)
    {
        addLabelledEdgeImpl({},from, pin, to, true);
    }

    // Return total fanout of device, across all pins (I think)
    uint32_t fanOut(PDeviceId id)
    {
        uint32_t acc=0;
        for(const auto & c : devices.at(id)->outgoing ){
            acc += c.size();
        }
        return acc;
    }

    // Return total fanin of device, across all pins (I think)
    uint32_t fanIn(PDeviceId id)
    {
        return devices.at(id)->incoming.size();
    }

    uint32_t getMaxFanOut() const
    { return m_maxFanOut; }

    uint32_t getMaxFanIn() const
    { return m_maxFanIn; }

    uint64_t getEdgeCount() const
    { return m_edgeCount; }

    // No-op for sw
    void map()
    {
        assert(!is_running_copy);
    }

    void write(HostLink *h)
    {
        assert(!is_running_copy);
        assert(h->m_graph==0);
        PGraphBase *copy=clone();
        h->attach_graph(copy);
        m_hostlink=h;
    }

private:

    struct transit_msg
    {
        unsigned dst;
        unsigned src;
        unsigned key;
        unsigned time;
        M msg;
    };

    bool is_running_copy=false;

    unsigned time_now=0;
    unsigned max_time_skew=0;
    uint64_t messages_sent=0;
    uint64_t messages_received=0;
    unsigned messages_in_flight_total=0;
    uint64_t sum_messages_in_flight_since_last_print=0;
    unsigned max_in_flight_ever=0;
    unsigned next_print_time=10000;
    unsigned time_since_print=0;
    std::deque<std::vector<transit_msg>> messages_in_flight;
    std::geometric_distribution<> msg_delay_distribution{0.1};

    bool deliver_out_of_order=true;

    void post_message(std::mt19937_64 &rng, unsigned dst, unsigned src, unsigned key, const M &msg)
    {
        assert(is_running_copy);
        unsigned distance;
        if(deliver_out_of_order){
            distance=msg_delay_distribution(rng);
        }else{
            distance=1;
        }
        while(messages_in_flight.size() <= distance){
            messages_in_flight.push_back({});
        }

        messages_in_flight.at(distance).push_back({dst, src, key, time_now, msg});
        messages_in_flight_total ++;
        messages_sent++;
    }
public:

    virtual void sim_prepare()
    {
        assert(is_running_copy);
        device_states.resize(numDevices);
        for(unsigned i=0; i<numDevices; i++){
            device_states[i].s = &devices[i]->state;
            device_states[i].readyToSend = &device_states[i]._realReadyToSend;
            device_states[i].numVertices=numDevices; // ?
            device_states[i].time=0; // ?

            device_states[i].init();
        }
    }

    virtual bool sim_step(
        std::mt19937_64 &rng,
        std::function<void (void *, size_t)> send_cb,
        std::function<bool (uint32_t &, size_t &, void *)> recv_cb
    ){
        assert(is_running_copy);
        if(time_now >= next_print_time){
            if(verbosity >= 1){
                double avg_in_flight=sum_messages_in_flight_since_last_print / (double)time_since_print;
                fprintf(stderr, "POLiteSWSim::PGraph::sim_step : Info - step=%u, sent=%llu, recv=%llu, in_flight:[now=%u, avg=%.1f, max=%u], max_skew=%u\n",
                        time_now, (unsigned long long)messages_sent, (unsigned long long)messages_received, messages_in_flight_total, avg_in_flight, max_in_flight_ever, max_time_skew);
            }
            next_print_time=(next_print_time*4)/3;
            sum_messages_in_flight_since_last_print=0;
            time_since_print=0;
        }
        max_in_flight_ever=std::max(max_in_flight_ever, messages_in_flight_total);
        sum_messages_in_flight_since_last_print += messages_in_flight_total;
        time_since_print++;

        bool idle=true;
        for(unsigned i=0; i<numDevices; i++){
            auto &d=device_states[i];
            if(d._realReadyToSend.index){
                if((rng()&1)==0){
                    PPin pin=d._realReadyToSend;

                    M msg;
                    d.send(&msg);

                    if(pin==No){
                        assert(false); // We only get in here if it was ready to send in the first place
                    }else if(pin==HostPin){
                        send_cb(&msg, sizeof(msg));
                    }else{
                        for(const auto &e : devices[i]->outgoing.at(pin.index-2)){
                            post_message(rng, e.first, i, e.second, msg);
                        }
                    }
                }
                idle=false;
            }
        }

        for(int i=0; i<10; i++){
            char buffer[1<<LogBytesPerMsg];
            uint32_t dst;
            size_t size;
            if(!recv_cb(dst, size, buffer)){
                break;
            }
            if(size!=sizeof(M)){
                throw std::runtime_error("Host sent message that did not match size of message type.");
            }
            M msg;
            memcpy(&msg, buffer, sizeof(M));
            post_message(rng, dst, -1, -1, msg);
        }

        if(!messages_in_flight.empty()){
            const auto &now=messages_in_flight.front();
            for(const transit_msg &m : now){
                unsigned time_skew=time_now - m.time;
                if(time_skew > max_time_skew){
                    max_time_skew=time_skew;
                }
                if(m.src==(unsigned)-1){ // from host
                    assert(m.key==(unsigned)-1);
                    device_states[m.dst].recv((M*)&m.msg, nullptr);
                }else{ // from device
                    device_states[m.dst].recv((M*)&m.msg, &devices[m.dst]->incoming[m.key]);
                }
            }
            messages_in_flight_total -= now.size();
            messages_received += now.size();
            messages_in_flight.pop_front();
            idle=false;
        }

        time_now++;

        if(!idle){
            return true;
        }

        bool any_active=false;
        for(unsigned i=0; i<numDevices; i++){
            any_active |= device_states[i].step();
            device_states[i].time++;
        }
        if(any_active){
            return true;
        }

        time_now++;

        for(unsigned i=0; i<numDevices; i++){
            M msg;
            if(device_states[i].finish(&msg)){
                send_cb(&msg, sizeof(M));
            }
        }

        return false;
    }
};

}; // namespace POLiteSWSim

#endif 