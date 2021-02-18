#ifndef PGraph_h
#define PGraph_h

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
typedef uint8_t PPin;
const PPin No = 0;
const PPin HostPin = 1;
inline PPin Pin(unsigned n){
    assert(n<POLITE_NUM_PINS);
    return ((n)+2);
}

const unsigned  TinselLogWordsPerMsg = 16;
const unsigned TinselLogBytesPerWord = 2;



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

class HostLink
{
private:
    std::mutex m_mutex;
    std::condition_variable m_cond;

    std::deque<std::vector<uint8_t>> m_dev2host;

    std::atomic<bool> m_user_waiting;
    std::atomic<bool> m_worker_interrupt;
    std::atomic<bool> m_worker_running;

    void worker_proc()
    {
        // To make debugging easier we hold the lock and occasionally release
        // it, so that effectively only one thread is running
        std::unique_lock<std::mutex> lk(m_mutex);

        std::mt19937_64 rng;
        rng.seed(time(0));

        std::function<void (void *,size_t)> send_cb=[&](void *p, size_t n)
        {
            //fprintf(stderr, "Sending host message\n");
            assert(lk.owns_lock());
            std::vector<uint8_t> m((char*)p, n+(char*)p);
            m_dev2host.push_back(m);
        };

        m_graph->sim_prepare();

        while(1){
            if(!m_graph->sim_step(rng, send_cb)){
                fprintf(stderr, "Exitig worker threads due to devices finishing.\n");
                break;
            }

            if(m_worker_interrupt.load()){
                fprintf(stderr, "Exiting worker thread due to interrupt\n");
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
    void boot(const char *code, const char *data)
    {};

    // Needs to create an _independent_ thread which runs in the 
    // background, then return
    void go()
    {
        m_worker_interrupt.store(false);
        m_worker_running.store(true);
        m_worker=std::thread([=](){ worker_proc(); });
    }

    // Blocking receive of message
    void recvMsg(void* msg, uint32_t numBytes)
    {
        m_user_waiting.store(true);

        std::unique_lock<std::mutex> lk(m_mutex);

        //fprintf(stderr, "Waitig for host message.");

        m_cond.wait(lk, [&](){
            return !m_dev2host.empty() || !m_worker_running.load();
        });

        if(m_dev2host.empty()){
            fprintf(stderr, "Error : recvMsg was called, but devices finished before message wasgenerated.");
            exit(1);
        }

       // fprintf(stderr, "Got host message.");
        
        auto front=m_dev2host.front();
        m_dev2host.pop_front();

        if(numBytes!=front.size()){
            throw std::runtime_error("Got wrong size message.");
        }
        memcpy(msg, &front[0], numBytes);

        m_user_waiting.store(false);

        m_cond.notify_all();
    }

    void recv(void* msg)
    {
        recvMsg(msg, 1<<(TinselLogBytesPerWord+TinselLogWordsPerMsg));
    }
};

// No-op for SW
inline void politeSaveStats(HostLink* hostLink, const char* filename)
{}

template<class M>
struct PMessage
{
    M payload;
};

template <typename S, typename E, typename M>
struct PDevice {
    // Impementation
    PPin _realReadyToSend;

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
class PGraph
    : public PGraphBase // Implementation detail
{
private:
    HostLink *m_hostlink;
public:
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

        // User visible stuff
        S state;
    };

    PDeviceId newDevice()
    {
        PDeviceId id=numDevices++;
        devices.push_back(std::make_shared<PState>());
        memset(&devices.back()->state, 0, sizeof(S));
        return id;
    }

    void addEdge(PDeviceId from, PinId pin, PDeviceId to)
    {
        addLabelledEdge({}, from, pin, to);
    }

    void addLabelledEdge(E edge, PDeviceId from, PinId pin, PDeviceId to)
    {
        assert(pin<POLITE_NUM_PINS);
        std::shared_ptr<PState> dstDev=devices.at(to);
        std::shared_ptr<PState> srcDev=devices.at(from);
        unsigned key=dstDev->incoming.size();      
        dstDev->incoming.push_back(edge);
        srcDev->outgoing[pin+2].push_back({to,key});  
    }

    bool mapVerticesToDRAM=false; // Dummy flag
    bool mapInEdgesToDRAM=false; // Dummy flag
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
    std::vector<std::tuple<PDeviceId,unsigned,M>> messages_in_flight;
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
        bool idle=true;
        for(unsigned i=0; i<numDevices; i++){
            auto &d=device_states[i];
            if(d._realReadyToSend){
                if(rng()&1){
                    PPin pin=d._realReadyToSend;

                    M msg;
                    d.send(&msg);

                    if(pin==HostPin){
                        //fprintf(stderr, "Send(%u) -> Host\n", i);
                        send_cb(&msg, sizeof(M));
                    }else{
                        //fprintf(stderr, "Send(%u) -> Pin(%d)\n", i, pin-2);
                        for(const auto &e : devices[i]->outgoing[pin]){
                            messages_in_flight.push_back({e.first,e.second,msg});
                        }
                    }
                }
                idle=false;
            }
        }

        if(!messages_in_flight.empty()){
            std::vector<std::tuple<PDeviceId,unsigned,M>> next_msgs;
            for(unsigned i=0; i<messages_in_flight.size(); i++){
                if(rng()&1){
                    PDeviceId dstId=std::get<0>(messages_in_flight[i]);
                    unsigned key=std::get<1>(messages_in_flight[i]);
                    M msg=std::get<2>(messages_in_flight[i]);

                    E e=devices.at(dstId)->incoming.at(key);

                    device_states[dstId].recv(&msg, &e);
                }else{
                    next_msgs.push_back(messages_in_flight[i]);
                }
            }
            std::swap(messages_in_flight, next_msgs);
            idle=false;
        }

        if(!idle){
            //fprintf(stderr, "Not idle\n");
            return true;
        }

        bool any_active=false;
        for(unsigned i=0; i<numDevices; i++){
            any_active |= device_states[i].step();
        }
        if(any_active){
            return true;
        }

        for(unsigned i=0; i<numDevices; i++){
            M msg;
            if(device_states[i].finish(&msg)){
                send_cb(&msg, sizeof(M));
            }
        }

        return false;
    }
};

#endif 