
#ifndef PerfCounterAccumulator_h
#define PerfCounterAccumulator_h

#include <cstdio>
#include <cstdint>
#include <vector>
#include <unordered_map>

template<class TGraph,class TThread>
class PolitePerfCounterAccumulator
{
public:
  static constexpr uint32_t INVALID32 = 0xFFFFFFFFul;
  static constexpr uint64_t INVALID64 = 0xFFFFFFFFFFFFFFFFull;

  using Impl = typename TThread::Impl;

  struct thread_perf_counters_t
  {
    uint32_t deviceId;
    uint32_t threadId;
    uint32_t messagesSent = INVALID32;
    uint32_t sendHandlerCalls = INVALID32;
    uint32_t totalSendHandlerTime = INVALID32;
    uint32_t blockedSends = INVALID32;
    uint32_t messagesReceived = INVALID32;
    uint32_t totalRecvHandlerTime = INVALID32;
    uint32_t minBarrierActiveTime = INVALID32;
    uint32_t sumBarrierActiveTimeDiv256 = INVALID32;
    uint32_t maxBarrierActiveTime = INVALID32;
  };

  struct cache_perf_counters_t
  {
    uint32_t cacheId;
    uint32_t hitCount = INVALID32;
    uint32_t missCount = INVALID32;
    uint32_t writebackCount = INVALID32;
  };

  struct core_perf_counters_t
  {
    uint32_t coreId;
    union{
      struct{
        uint32_t cycleCountHi;
        uint32_t cycleCountLo;
      };
      uint64_t cycleCount;
    };
    union{
      struct{
        uint32_t idleCountHi;
        uint32_t idleCountLo;
      };
      uint64_t idleCount;
    };
  };

  struct board_perf_counters_t
  {
    uint32_t boardId;
    uint16_t boardX, boardY;
    uint32_t progRouterSent = INVALID32;
    uint32_t progRouterSentInter = INVALID32;
  };

  struct device_performance_counters_t
  {
    uint32_t deviceId;
    uint32_t threadId;
    std::array<uint32_t,TThread::DeviceType::NumDevicePerfCounters> device_performance_counters;
  };
  
  struct combined_device_perf_counters_t
  {
    board_perf_counters_t board;
    cache_perf_counters_t cache;
    core_perf_counters_t core;
    thread_perf_counters_t thread;
    device_performance_counters_t device;
  };

  struct combined_thread_perf_counters_t
  {
    board_perf_counters_t board;
    cache_perf_counters_t cache;
    core_perf_counters_t core;
    thread_perf_counters_t thread;
  };

  struct combined_core_perf_counters_t
  {
    board_perf_counters_t board;
    cache_perf_counters_t cache;
    core_perf_counters_t core;
  };

private:
  bool m_complete;
  TGraph &m_graph;

  std::vector<device_performance_counters_t> m_device_perf_counters;
  std::unordered_map<uint32_t,thread_perf_counters_t> m_thread_perf_counters;
  std::unordered_map<uint32_t,cache_perf_counters_t> m_cache_perf_counters;
  std::unordered_map<uint32_t,core_perf_counters_t> m_core_perf_counters;
  std::unordered_map<uint32_t,board_perf_counters_t> m_board_perf_counters;
  uint32_t m_received_counters;
  uint32_t m_expected_counters;
public:
    template<class THostLink>
  PolitePerfCounterAccumulator(TGraph &graph, THostLink *hostLink)
    : m_graph(graph)
    , m_received_counters(0)
    , m_expected_counters(0)
  {
    fprintf(stderr, "HasDevicePerfCounters=%d\n", TThread::DeviceType::HasDevicePerfCounters);

    if(TThread::DeviceType::HasDevicePerfCounters){
      m_device_perf_counters.resize( graph.numDevices );
      for(unsigned i=0; i<m_device_perf_counters.size(); i++){
        m_device_perf_counters[i].deviceId=i;
        m_device_perf_counters[i].threadId=graph.getThreadIdFromDeviceId(i);
        m_device_perf_counters[i].device_performance_counters.fill(INVALID32);
      }
      m_expected_counters += graph.numDevices * TThread::DeviceType::NumDevicePerfCounters;
    }
    if(TThread::ENABLE_THREAD_PERF_COUNTERS || TThread::ENABLE_CORE_PERF_COUNTERS){
      uint32_t cacheMask = (1 << (Impl::LogThreadsPerCore + Impl::LogCoresPerDCache)) - 1;

      for (unsigned x = 0; x < graph.getMeshLenX(); x++) {
        for (unsigned y = 0; y < graph.getMeshLenY(); y++) {
          for (int c = 0; c < Impl::CoresPerBoard; c++) {
            for(uint32_t t = 0 ; t < Impl::ThreadsPerCore; t++){
              uint32_t threadId=hostLink->toAddr(x, y, c, t);
              if(TThread::ENABLE_THREAD_PERF_COUNTERS){
                m_thread_perf_counters[threadId].threadId=threadId;
                m_expected_counters += 9;
              }
              if(TThread::ENABLE_CORE_PERF_COUNTERS){
                if(t==0){
                  auto &cc=m_core_perf_counters[threadId];
                  cc.coreId=threadId;
                  cc.cycleCountHi=INVALID32;
                  cc.cycleCountLo=INVALID32;
                  cc.idleCountHi=INVALID32;
                  cc.idleCountLo=INVALID32;
                  m_expected_counters += 4;
                }
                if((threadId&cacheMask)==0){
                  m_cache_perf_counters[threadId].cacheId=threadId;
                  m_expected_counters += 3;
                }
                if(c==0 && t==0){
                  m_board_perf_counters[threadId].boardId=threadId;
                  m_board_perf_counters[threadId].boardX=x;
                  m_board_perf_counters[threadId].boardY=y;
                  m_expected_counters += 2;
                }
              }
            }
          }
        }
      }
    }
  }

  bool is_complete() const
  { return m_received_counters==m_expected_counters; }

  void print_progress() const
  {
    fprintf(stderr, "Received %u out of %u\n", m_received_counters, m_expected_counters);
  }

  combined_core_perf_counters_t get_combined_core_counters(uint32_t threadId) const
  {
    static const uint32_t CORE_MASK= ~ uint32_t((1ul << (Impl::LogThreadsPerCore)) - 1);
    if(threadId & ~CORE_MASK){
      throw std::runtime_error("Thread is not a core thread.");
    }

    static const uint32_t CACHE_MASK= ~ uint32_t((1ul << (Impl::TinselLogThreadsPerCore + Impl::TinselLogCoresPerDCache)) - 1);
    static const uint32_t BOARD_MASK = ~ uint32_t((1ul << (Impl::TinselLogThreadsPerBoard)) - 1);

    combined_core_perf_counters_t res;
    res.core=m_core_perf_counters.at(threadId );
    res.cache=m_cache_perf_counters.at(threadId & CACHE_MASK);
    res.board=m_board_perf_counters.at(threadId & BOARD_MASK);
    return res;
  }

  combined_thread_perf_counters_t get_combined_thread_counters(uint32_t threadId) const
  {
    static const uint32_t CORE_MASK= ~ uint32_t((1ul << (Impl::LogThreadsPerCore)) - 1);
    static const uint32_t CACHE_MASK= ~ uint32_t((1ul << (Impl::LogThreadsPerCore + Impl::LogCoresPerDCache)) - 1);
    static const uint32_t BOARD_MASK = ~ uint32_t((1ul << (Impl::LogThreadsPerBoard)) - 1);

    combined_thread_perf_counters_t res;
    res.thread=m_thread_perf_counters.at(threadId);
    res.core=m_core_perf_counters.at(threadId & CORE_MASK);
    res.cache=m_cache_perf_counters.at(threadId & CACHE_MASK);
    res.board=m_board_perf_counters.at(threadId & BOARD_MASK);
    return res;
  }

  combined_device_perf_counters_t get_combined_device_counters(uint32_t deviceId) const
  {
    combined_device_perf_counters_t res;
    res.device=m_device_perf_counters.at(deviceId);

    combined_thread_perf_counters_t tres=get_combined_thread_counters(res.device.threadId);
    res.thread=tres.thread;
    res.board=tres.board;
    res.core=tres.core;
    res.cache=tres.cache;
    return res;
  }


  void enum_combined_device_counters(const std::function<void(const combined_device_perf_counters_t)> &cb)
  {
    for(unsigned i=0; i<m_device_perf_counters.size(); i++){
      cb( get_combined_device_counters(i) );
    }
  }

  void dump_combined_device_counters(const std::vector<std::string> &device_perf_counter_names,FILE *dst)
  {
    fprintf(dst, "DeviceId,ThreadId,CoreId,CacheId,BoardId,BoardX,BoardY");
    if(TThread::ENABLE_CORE_PERF_COUNTERS){
      fprintf(dst, ",boProgRouterSent,boProgRouterSentInter");
      fprintf(dst, ",caHitCount,caMissCount,caWritebackCount");
      fprintf(dst, ",coCycleCount,coIdleCount");
    }
    if(TThread::ENABLE_THREAD_PERF_COUNTERS){
      fprintf(dst, ",thSendHandlerCalls,thSendHandlerTime,thMessagesSent,theBlockedSend");
      fprintf(dst, ",thMessagesRecv,thRecvHandlerTime");
      fprintf(dst, ",thMinBarrierActive,thSumBarrierActive,thMaxBarrierActive");
    }
    if(TThread::DeviceType::HasDevicePerfCounters){
      if(device_perf_counter_names.size() != TThread::DeviceType::NumDevicePerfCounters){
        throw std::runtime_error("Mis-match between device perf counter name counts.");
      }
      for(unsigned i=0; i<device_perf_counter_names.size(); i++){
        fprintf(dst, ",%s", device_perf_counter_names[i].c_str());
      }
    }
    fprintf(dst, "\n");

    for(unsigned i=0; i<m_device_perf_counters.size(); i++){
      auto c=get_combined_device_counters(i);
      fprintf(dst,"%u,%u,%u,%u,%u,%u,%u", c.device.deviceId, c.thread.threadId,
        c.core.coreId, c.cache.cacheId, c.board.boardId, c.board.boardX, c.board.boardY);
      
      if(TThread::ENABLE_CORE_PERF_COUNTERS){
        fprintf(dst, ",%u,%u", c.board.progRouterSent,c.board.progRouterSentInter);
        fprintf(dst, ",%u,%u,%u", c.cache.hitCount,c.cache.missCount,c.cache.writebackCount);
        fprintf(dst, ",%llu,%llu", (unsigned long long)c.core.cycleCount, (unsigned long long)c.core.idleCount);
      }

      if(TThread::ENABLE_THREAD_PERF_COUNTERS){
        fprintf(dst, ",%u,%u,%u,%u", c.thread.sendHandlerCalls, c.thread.totalSendHandlerTime, c.thread.messagesSent, c.thread.blockedSends);
        fprintf(dst, ",%u,%u,%u", c.thread.messagesReceived, c.thread.totalRecvHandlerTime, c.thread.minBarrierActiveTime);
        fprintf(dst, ",%llu,%u", c.thread.sumBarrierActiveTimeDiv256*256ull, c.thread.maxBarrierActiveTime);
      }

      if(TThread::DeviceType::HasDevicePerfCounters){
        for(unsigned j=0; j<device_perf_counter_names.size(); j++){
          fprintf(dst, ",%u", c.device.device_performance_counters[j]);
        }
      }
      fprintf(dst,"\n");
    }
  }

  void dump_combined_thread_counters(FILE *dst)
  {
    fprintf(dst, "ThreadId,CoreId,CacheId,BoardId,BoardX,BoardY");
    if(TThread::ENABLE_CORE_PERF_COUNTERS){
      fprintf(dst, ",boProgRouterSent,boProgRouterSentInter");
      fprintf(dst, ",caHitCount,caMissCount,caWritebackCount");
      fprintf(dst, ",coCycleCount,coIdleCount");
    }
    if(TThread::ENABLE_THREAD_PERF_COUNTERS){
      fprintf(dst, ",thSendHandlerCalls,thSendHandlerTime,thMessagesSent,theBlockedSend");
      fprintf(dst, ",thMessagesRecv,thRecvHandlerTime");
      fprintf(dst, ",thMinBarrierActive,thSumBarrierActive,thMaxBarrierActive");
    }
    fprintf(dst, "\n");

    for(unsigned i=0; i<m_thread_perf_counters.size(); i++){
      auto c=get_combined_thread_counters(i);
      fprintf(dst,"%u,%u,%u,%u,%u,%u", c.thread.threadId,
        c.core.coreId, c.cache.cacheId, c.board.boardId, c.board.boardX, c.board.boardY);
      
      if(TThread::ENABLE_CORE_PERF_COUNTERS){
        fprintf(dst, ",%u,%u", c.board.progRouterSent,c.board.progRouterSentInter);
        fprintf(dst, ",%u,%u,%u", c.cache.hitCount,c.cache.missCount,c.cache.writebackCount);
        fprintf(dst, ",%llu,%llu", (unsigned long long)c.core.cycleCount, (unsigned long long)c.core.idleCount);
      }

      if(TThread::ENABLE_THREAD_PERF_COUNTERS){
        fprintf(dst, ",%u,%u,%u,%u", c.thread.sendHandlerCalls, c.thread.totalSendHandlerTime, c.thread.messagesSent, c.thread.blockedSends);
        fprintf(dst, ",%u,%u,%u", c.thread.messagesReceived, c.thread.totalRecvHandlerTime, c.thread.minBarrierActiveTime);
        fprintf(dst, ",%llu,%u", c.thread.sumBarrierActiveTimeDiv256*256ull, c.thread.maxBarrierActiveTime);
      }
      fprintf(dst,"\n");
    }
  }

  bool process_line(uint32_t threadId, const char *line)
  {
    // Pattern is: "feature:threadId,group,key,value\n",  threadId,deviceOffset,counterOffset,counterValue

    char name[9]={0};
    unsigned gotThreadId, group, key, value;
    unsigned g=sscanf(line, "%8[^:]:%x,%x,%x,%x", name, &gotThreadId, &group, &key, &value);
    if(g!=5){
      return false;
    }

    auto require=[](bool cond, const char *msg)
    {
      if(!cond){
        throw std::runtime_error(std::string("Error while parsing perf counters : ")+msg);
      }
    };

    unsigned num_hits=0;

    auto cond_assign_valid=[&](bool cond, uint32_t &dst, uint32_t value)
    {
      if(cond){
        require(dst == INVALID32, "Duplicate perf counter received.");
        dst= value;
        ++num_hits;
        require(num_hits==1, "Perf counter parsing code got two hits.");
      }
    };

    require(threadId==gotThreadId, "DebugLink threadId does not match expected thread id.");

    if(!strcmp(name,"DPC")){
      auto deviceId=m_graph.getDeviceId(threadId, group);
      require(deviceId < m_graph.numDevices, "Invalid device index.");

      cond_assign_valid(true, m_device_perf_counters.at(deviceId).device_performance_counters[key], value);
    }else if(!strcmp(name, "ThPC")){
      auto &th=m_thread_perf_counters.at(threadId);
      cond_assign_valid( key==TThread::SendHandlerCalls, th.sendHandlerCalls, value);
      cond_assign_valid( key==TThread::TotalSendHandlerTime, th.totalSendHandlerTime, value);
      cond_assign_valid( key==TThread::BlockedSends, th.blockedSends, value);
      cond_assign_valid( key==TThread::MsgsSent, th.messagesSent, value);
      
      cond_assign_valid( key==TThread::MsgsRecv, th.messagesReceived, value);
      cond_assign_valid( key==TThread::TotalRecvHandlerTime, th.totalRecvHandlerTime, value);

      cond_assign_valid( key==TThread::MinBarrierActive, th.minBarrierActiveTime, value);
      cond_assign_valid( key==TThread::MaxBarrierActive, th.maxBarrierActiveTime, value);
      cond_assign_valid( key==TThread::SumBarrierActiveDiv256, th.sumBarrierActiveTimeDiv256, value);
      
    }else if(!strcmp(name, "CoPC")){
      auto &co=m_core_perf_counters.at(threadId);

      cond_assign_valid( key==0, co.cycleCountHi, value);
      cond_assign_valid( key==1, co.cycleCountLo, value);
      cond_assign_valid( key==2, co.idleCountHi, value);
      cond_assign_valid( key==3, co.idleCountLo, value);
    }else if(!strcmp(name, "CaPC")){
      auto &ca=m_cache_perf_counters.at(threadId);
      cond_assign_valid( key==0, ca.hitCount, value);
      cond_assign_valid( key==1, ca.missCount, value);
      cond_assign_valid( key==2, ca.writebackCount, value);
    }else if(!strcmp(name, "BoPC")){
      auto &bo=m_board_perf_counters.at(threadId);
      cond_assign_valid( key==0, bo.progRouterSent, value);
      cond_assign_valid( key==1, bo.progRouterSentInter, value);
    }else{
      return false;
    }

    if(num_hits==0){
      fprintf(stderr, "Line = %s\n", line);
      require(num_hits==1, "Perf counter code got no hits.");
    }

    m_received_counters++;

    return true;
  }
};

#endif
