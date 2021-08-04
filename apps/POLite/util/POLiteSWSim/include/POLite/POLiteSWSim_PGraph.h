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

struct POLiteSWSim {

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

static const unsigned  LogWordsPerMsg = 4;
static const unsigned  LogBytesPerMsg = 6;
static const unsigned LogBytesPerWord = 2;
static const unsigned LogBytesPerFlit = 4;
static const unsigned CoresPerFPU=32;
static const unsigned LogBytesPerDRAM=27;
static const unsigned MeshXBits=3;
static const unsigned MeshYBits=3;
static const unsigned BoxMeshXLen=4;
static const unsigned BoxMeshYLen=4;

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

    virtual void sim_prepare() =0;

    virtual bool sim_step(
        std::mt19937_64 &rng,
        std::function<void (void *, size_t)> send_cb
    ) =0;
};

// HostLink parameters
struct HostLinkParams {
  uint32_t numBoxesX;
  uint32_t numBoxesY;
  bool useExtraSendSlot;

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
private:
    std::mutex m_mutex;
    std::condition_variable m_cond;

    std::deque<std::vector<uint8_t>> m_dev2host;

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
        rng.seed(time(0));

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

        m_graph->sim_prepare();

        while(1){
            if(!m_graph->sim_step(rng, send_cb)){
                if(verbosity>=2){
                    fprintf(stderr, "POLiteSWSim::HostLink : Info - Exiting device worker thread due to devices finishing.\n");
                }
                break;
            }

            if(m_worker_interrupt.load()){
                if(verbosity>=2){
                    fprintf(stderr, "POLiteSWSim::HostLink : Info - Exiting worker thread due to interrupt (host finishing while devices still running, probably fine).\n");
                }
                break;
            }
            if( (m_user_waiting.load() && !m_dev2host.empty()) ){
                m_cond.notify_all();
                m_cond.wait(lk);
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
    {}

    ~HostLink()
    {
        m_worker_interrupt.store(true);
        if(m_worker.joinable()){
            m_worker.join();
        }
    }

    // Implementation detail
    PGraphBase *m_graph=0;

    /* Used to work around destruction order. If the graph is destroyed before the
        HostLink (which is common), we need to end the processing loop. */
    void detach_graph(PGraphBase *graph)
    {
        assert(graph==m_graph);
        m_worker_interrupt.store(true);
        std::unique_lock<std::mutex> lk(m_mutex);

        m_cond.wait(lk, [&](){ return m_worker_running.load()==false; });
    }

    // No-op for SW
    void boot(const char */*code*/, const char */*data*/)
    {};

    // Needs to create an _independent_ thread which runs in the 
    // background, then return
    void go()
    {
        m_worker_interrupt.store(false);
        m_worker_running.store(true);
        m_worker=std::thread([=](){ worker_proc(); });
    }

    // Blocking receive of max size message
    void recvMsg(void* msg, uint32_t numBytes)
    {
        if(numBytes > (1<<LogBytesPerMsg)){
            fprintf(stderr, "POLiteSWSim::HostLink::recvMsg : Error - Attempt to read more than max sized message.");
            exit(1);
        }

        m_user_waiting.store(true);

        std::unique_lock<std::mutex> lk(m_mutex);

        //fprintf(stderr, "Waitig for host message.");

        m_cond.wait(lk, [&](){
            return !m_dev2host.empty() || !m_worker_running.load();
        });

        if(m_dev2host.empty()){
            fprintf(stderr, "POLiteSWSim::HostLink::recvMsg : Error - HostLink::recvMsg was called, but devices have all finished and there are no pending messages - app will block.");
            exit(1);
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

        m_user_waiting.store(false);

        m_cond.notify_all();
    }

    // Blocking receive of max size message
    void recv(void* msg)
    {
        recvMsg(msg, 1<<LogBytesPerMsg);
    }
};

// No-op for SW
inline static void politeSaveStats(HostLink* /*hostLink*/, const char* /*filename*/)
{}



template <typename S, typename E, typename M>
struct PDevice {
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

template <typename DeviceType, typename S, typename E, typename M>
struct PThread {

};

template <typename DeviceType, typename S, typename E, typename M>
class PGraph
    : public PGraphBase // Implementation detail
{
private:
    HostLink *m_hostlink;

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

    PGraph()
    {
        deliver_out_of_order=POLiteSWSim::get_option_bool("POLITE_SW_SIM_DELIVER_OUT_OF_ORDER", true);
        verbosity=POLiteSWSim::get_option_unsigned("POLITE_SW_SIM_VERBOSITY", 1);
    }

    ~PGraph()
    {
        if(m_hostlink){
            m_hostlink->detach_graph(this);
            m_hostlink=0;
        }
    }

    // This structure must be directly exposed to clients
    struct PState
    {
        // Implementation stuff
        // For outgoing we keep that 0==empty and 1==host
        std::array<std::vector<std::pair<PDeviceId,unsigned>>,POLITE_NUM_PINS+2> outgoing;
        std::vector<E> incoming;
        unsigned fanOut=0;

        // User visible stuff
        S state;

        std::mutex lock;
    };

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

    void reserveOutgoingEdgeSpace(PDeviceId from, PinId pin, unsigned n)
    {
        auto &d=devices.at(from)->outgoing[pin];
        d.reserve(d.size()+n);
    }


    void addEdge(PDeviceId from, PinId pin, PDeviceId to)
    {
        addLabelledEdge({}, from, pin, to);
    }

    void addLabelledEdgeImpl(E edge, PDeviceId from, PinId pin, PDeviceId to, bool lock_dst)
    {
        assert(pin<POLITE_NUM_PINS);
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

    bool mapVerticesToDRAM=false; // Dummy flag
    bool mapInEdgeHeadersToDRAM=false; // Dummy flag
    bool mapInEdgeRestToDRAM=false; // Dummy flag
    bool mapOutEdgesToDRAM=false; // Dummy flag

    // uint32_t i = graph.numDevices;
    uint32_t numDevices = 0;

    // S &s = graph.devices[i]->state;
    std::vector<std::shared_ptr<PState>> devices;

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
    {}

    void write(HostLink *h)
    {
        assert(h->m_graph==0);
        h->m_graph=this;
        m_hostlink=h;
    }

private:
    std::vector<DeviceType> device_states;

    struct transit_msg
    {
        unsigned dst;
        unsigned src;
        unsigned key;
        unsigned time;
        M msg;
    };

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

    bool deliver_out_of_order;

    void post_message(std::mt19937_64 &rng, unsigned dst, unsigned src, unsigned key, const M &msg)
    {
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
        std::function<void (void *, size_t)> send_cb
    ){
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

        if(!messages_in_flight.empty()){
            const auto &now=messages_in_flight.front();
            for(const transit_msg &m : now){
                unsigned time_skew=time_now - m.time;
                if(time_skew > max_time_skew){
                    max_time_skew=time_skew;
                }
                device_states[m.dst].recv((M*)&m.msg, &devices[m.dst]->incoming[m.key]);
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