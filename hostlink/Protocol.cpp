#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <boot.h>
#include <config.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include "HostLink.h"

#include <map>
#include <string>
#include <vector>
#include <algorithm>

extern double now();

class Protocol
{
private:
  enum Tag
    {
      TagStdOut = 0x01,
      TagStdErr = 0x02,
      TagAssertRich = 0xFD,
      TagAssert = 0xFE,
      TagExit   = 0xFF,
      TagKeyVal = 0x10,
      TagPerfmon = 0x20
    };
  
  enum State
    {
      StateIdle,
      StateStdOut,
      StateStdErr,
      StateExit,
      StatePerfmon_thread,
      StatePerfmon_blocked,
      StatePerfmon_idle,
      StatePerfmon_perfmon,
      StatePerfmon_send,
      StatePerfmon_send_handler,
      StatePerfmon_recv,
      StatePerfmon_recv_handler,
      StateKeyVal_Device,
      StateKeyVal_Key,
      StateKeyVal_Val,
      StateAssertRich_File,
      StateAssertRich_Line
    }m_state;
  
  uint32_t m_threadId;

  std::vector<char> m_chars;
  std::vector<char> m_device;
  uint32_t m_key, m_val;
  unsigned m_todo;

  uint32_t m_thread_cycles;
  uint32_t m_blocked_cycles;
  uint32_t m_idle_cycles;
  uint32_t m_perfmon_cycles;
  uint32_t m_send_cycles;
  uint32_t m_send_handler_cycles;
  uint32_t m_recv_cycles;
  uint32_t m_recv_handler_cycles;

  // Using vector rather than std::string due to conflicts with Altera's UART libc++
  typedef   std::vector<std::pair<std::vector<char>,uint32_t> > key_val_map_t;
  key_val_map_t m_deviceKeyValSeq;

  FILE *m_keyValDst;
  FILE *m_measureDst;
  FILE *m_perfmonDst;

  unsigned m_totalBytes;
  unsigned m_totalStdoutBytes;
  unsigned m_totalExportedKeyValues;

  int m_verbosity;

  static bool lt(const std::pair<std::vector<char>,uint32_t> &a, const std::pair<std::vector<char>,uint32_t> &b)
  {
    return a.first < b.first;
  };


  // This is because maps don't seem to work (no, really!).
  // I think due to conflicts with the uart libc++. Or I'm dumb.
  unsigned incSeq(const std::vector<char> &name)
  {
    const std::pair<std::vector<char>,uint32_t> val(name,0);
    key_val_map_t::iterator it=std::upper_bound(m_deviceKeyValSeq.begin(), m_deviceKeyValSeq.end(), val, lt);
    if(it==m_deviceKeyValSeq.end() || it->first!=name){
      it=m_deviceKeyValSeq.insert(it,val);
    }
    assert(it->first==name);
    unsigned res=it->second;
    it->second++;
    return res;
  }
public:
  Protocol(uint32_t threadId, FILE *keyValDst, FILE *measureDst, FILE *perfmonDst, int verbosity)
    : m_state(StateIdle)
    , m_threadId(threadId)
    , m_key(0)
    , m_val(0)
    , m_todo(0)
    , m_thread_cycles(0)
    , m_blocked_cycles(0)
    , m_idle_cycles(0)
    , m_perfmon_cycles(0)
    , m_send_cycles(0)
    , m_send_handler_cycles(0)
    , m_recv_cycles(0)
    , m_recv_handler_cycles(0)
    , m_keyValDst(keyValDst)
    , m_measureDst(measureDst)
    , m_perfmonDst(perfmonDst)
    , m_totalBytes(0)
    , m_totalStdoutBytes(0)
    , m_totalExportedKeyValues(0)
    , m_verbosity(verbosity)
  {
  }

  unsigned getTotalBytes()
  { return m_totalBytes; }

  unsigned getTotalStdoutBytes()
  { return m_totalStdoutBytes; }

  unsigned getTotalExportedKeyValues()
  { return m_totalExportedKeyValues; }
  
  bool add(uint8_t byte, int &exitCode)
  {
    m_totalBytes++;
    
    switch(m_state)
      {
      case StateIdle:
	switch(byte)
	  {
	  case TagStdOut:{
	    if(m_verbosity>1){
	      fprintf(stderr, "%08x : Begin stdout\n", m_threadId);
	    }
	    m_chars.clear();
	    m_state=StateStdOut;
	    break;
	  }
	  case TagStdErr:{
	    m_chars.clear();
	    m_state=StateStdErr;
	    break;
	  }
	  case TagAssertRich:{
	    m_state=StateAssertRich_File;
	    m_chars.clear();
	    break;
	  }
	  case TagAssert:{
	    fprintf(stderr, "ERROR : received assert from thread 0x%x\n", m_threadId);
	    exit(1);
	    break;
	  }
 	  case TagExit:{
	    m_state=StateExit;
	    m_val=0;
	    m_todo=4;
	    break;
	  }
	  case TagPerfmon:{
	    m_thread_cycles = 0;
            m_todo = 4;
	    m_state=StatePerfmon_thread;
	    break;
	  }
	  case TagKeyVal:{
	    m_state=StateKeyVal_Device;
	    m_key=0;
	    m_val=0;
	    m_todo=0;
	    break;
	  }
	  default:{
	    fprintf(stderr, "ERROR : received control tag %x from thread 0x%x\n", byte, m_threadId);
	    exit(1);
	    break;
	  }
	  }
	break;

      case StatePerfmon_thread:
	m_thread_cycles=(m_thread_cycles>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
          m_todo = 4;
          m_blocked_cycles = 0;
          m_state = StatePerfmon_blocked;
        }
	break;
      case StatePerfmon_blocked:
	m_blocked_cycles=(m_blocked_cycles>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
          m_todo = 4;
          m_idle_cycles = 0;
          m_state = StatePerfmon_idle;
        }
	break;
      case StatePerfmon_idle:
	m_idle_cycles=(m_idle_cycles>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
          m_todo = 4;
          m_perfmon_cycles = 0;
          m_state = StatePerfmon_perfmon;
        }
	break;
      case StatePerfmon_perfmon:
	m_perfmon_cycles=(m_perfmon_cycles>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
          m_todo = 4;
          m_send_cycles = 0;
          m_state = StatePerfmon_send;
        }
	break;
      case StatePerfmon_send:
	m_send_cycles=(m_send_cycles>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
          m_todo = 4;
          m_send_handler_cycles = 0;
          m_state = StatePerfmon_send_handler;
        }
	break;
      case StatePerfmon_send_handler:
	m_send_handler_cycles=(m_send_handler_cycles>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
          m_todo = 4;
          m_recv_cycles = 0;
          m_state = StatePerfmon_recv;
        }
	break;
      case StatePerfmon_recv:
	m_recv_cycles=(m_recv_cycles>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
          m_todo = 4;
          m_recv_handler_cycles = 0;
          m_state = StatePerfmon_recv_handler;
        }
	break;
      case StatePerfmon_recv_handler:
	m_recv_handler_cycles=(m_recv_handler_cycles>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
            m_state = StateIdle; 
            //sanity checks
            if (m_thread_cycles < m_blocked_cycles || m_thread_cycles < m_idle_cycles || m_thread_cycles < m_perfmon_cycles || m_thread_cycles < m_send_cycles || m_thread_cycles < m_recv_cycles) 
                 fprintf(stdout, "ERROR - SANITY CHECK FAILED -");
            if( m_send_handler_cycles > m_send_cycles || m_recv_handler_cycles > m_recv_cycles)
                 fprintf(stdout, "ERROR - SANITY CHECK FAILED -");
            //output
            if(m_verbosity>1) {
	        fprintf(stdout, " %08x, %u, %u, %u, %u, %u, %u, %u, %u \n", m_threadId, m_thread_cycles, m_blocked_cycles, m_idle_cycles, m_perfmon_cycles, m_send_cycles, m_send_handler_cycles, m_recv_cycles, m_recv_handler_cycles );
            }
            if(m_perfmonDst) {
	        fprintf(m_perfmonDst, " %08x, %u, %u, %u, %u, %u, %u, %u, %u \n", m_threadId, m_thread_cycles, m_blocked_cycles, m_idle_cycles, m_perfmon_cycles, m_send_cycles, m_send_handler_cycles, m_recv_cycles, m_recv_handler_cycles );
            }
        }
	break;

      case StateExit:
	m_val=(m_val>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
	  exitCode=(int)m_val;
	  return false;
	}
	break;
      case StateAssertRich_File:
	m_chars.push_back((char)byte);
	if(byte==0){
	  m_state=StateAssertRich_Line;
	  m_todo=4;
	  m_val=0;
	}
	break;
      case StateAssertRich_Line:
	m_val=(m_val>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
	  fprintf(stderr, "ERROR : assert from thread 0x%x at %s:%u\n", m_threadId, &m_chars[0], m_val);
	  exit(1);
	}
	break;
      case StateStdOut:
	//fprintf(stderr, "%08x : add stdout '%c'\n", m_threadId, (char)byte);
	m_totalStdoutBytes++;
	m_chars.push_back((char)byte);
	if(byte==0){
	  fprintf(stdout, "%08x : StdOut : ", m_threadId);
	  fputs(&m_chars[0],stdout);
	  if(m_chars.size()<=1 || m_chars[m_chars.size()-2]!='\n'){
	    	fputs("\n",stdout);
	  }
	  if(m_verbosity>0){
	    fflush(stdout);
	  }
	  if(m_verbosity>0){
	    fprintf(stderr, "%08x : End stdout\n", m_threadId);
	  }
	  m_state=StateIdle;
	}
	break;
      case StateStdErr:
	m_chars.push_back((char)byte);
	if(byte==0){
	  fprintf(stdout, "%08x : StdErr : ", m_threadId);
	  fputs(&m_chars[0],stderr);
	  if(m_chars.size()<=1 || m_chars[m_chars.size()-2]!='\n'){
		fputs("\n",stderr);
	  }

	  m_state=StateIdle;
	}
	break;
      case StateKeyVal_Device:
	m_device.push_back(byte);
	if(!byte){
	  m_state=StateKeyVal_Key;
	  m_todo=4;
	}
	break;
      case StateKeyVal_Key:
	m_key = (m_key>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
	  m_state=StateKeyVal_Val;
	  m_todo=4;
	}
	break;
      case StateKeyVal_Val:
	m_val = (m_val>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
	  // TODO : Still seems completely broken.
	  m_totalExportedKeyValues++;
	  unsigned seq=incSeq(m_device);
	  if(m_verbosity>1){
	    fprintf(stdout, "KeyVal : %s, %u, %u, %u\n", &m_device[0], seq, m_key, m_val);
	  }
	  if(m_keyValDst){
	    fprintf(m_keyValDst, "%s, %u, %u, %u\n", &m_device[0], seq, m_key, m_val);
	  }
	  m_state=StateIdle;
	}
	break;
      default:
	fprintf(stderr, "ERROR: Unknown state.\n");
	exit(1);
 
      }
    return true;
  }
};

void protocol(HostLink *link, FILE *keyValDst, FILE *measureDst, FILE *perfmonDst, int verbosity)
{
  double start=now();
  
  std::vector<Protocol> states;

  for(unsigned i=0;i<TinselThreadsPerBoard;i++){
    states.push_back(Protocol(i, keyValDst, measureDst, perfmonDst, verbosity));
  }

  int exitCode=0;

  unsigned totalBytes=0;
  while(1){
    totalBytes++;
    
    uint32_t src, val;
    
    uint8_t cmd = link->get(&src, &val);
    uint32_t id = val >> 8;
    uint32_t ch = val & 0xff;
    if(verbosity>2){
      fprintf(stderr, "  cmd=%u, id=%u, ch=%u\n", cmd, id, ch);
    }
    assert(id < TinselThreadsPerBoard);

    if(!states[id].add(ch, exitCode)){
      break;
    }
    fflush(stdout);
  }

  double finish=now();

  fprintf(stderr, "Application exited : exitCode = %d, execTime=%f\n", exitCode, finish-start);

  if(measureDst){
    fprintf(measureDst, "appExitCode, -, %d, -\n", exitCode);
    fprintf(measureDst, "appWallClockTime, -, %f, sec\n", finish-start);
    unsigned totalExportedKeyValues=0;
    unsigned totalStdoutBytes=0;
    for(unsigned i=0; i<TinselThreadsPerBoard; i++){
      if (states[i].getTotalBytes()!=0){
	fprintf(measureDst, "deviceHostlinkRecv, %d, %d, bytes\n", i, states[i].getTotalBytes() );
      }
      if(states[i].getTotalStdoutBytes()!=0){
	fprintf(measureDst, "deviceStdoutRecv, %d, %d, bytes\n", i, states[i].getTotalStdoutBytes() );
      }
      if(states[i].getTotalExportedKeyValues()!=0){
	fprintf(measureDst, "deviceExportedCount, %d, %d, key-values\n", i, states[i].getTotalExportedKeyValues() );
      }
      totalExportedKeyValues+=states[i].getTotalExportedKeyValues();
      totalStdoutBytes+=states[i].getTotalExportedKeyValues();
    }
    fprintf(measureDst, "appHostlinkRecv, -, %d, bytes\n", totalBytes );
    fprintf(measureDst, "appStdoutRecv, -, %d, bytes\n", totalStdoutBytes );
    fprintf(measureDst, "appExportedCount, -, %d, key-values\n", totalExportedKeyValues );
    fflush(measureDst);
  }
  
  exit(exitCode);
}

