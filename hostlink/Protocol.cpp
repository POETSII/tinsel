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

#include <vector>


class Protocol
{
private:
  enum Tag
    {
      TagStdOut = 0x01,
      TagStdErr = 0x02,
      TagAssert = 0xFE,
      TagKeyVal = 0x10
    };
  
  enum State
    {
      StateIdle,
      StateStdOut,
      StateStdErr,
      StateAssert,
      StateKeyVal_Key,
      StateKeyVal_Val
    }m_state;
  
  uint32_t m_threadId;

  std::vector<char> m_chars;
  uint32_t m_key, m_val;
  unsigned m_todo;
public:
  Protocol(uint32_t threadId)
    : m_state(StateIdle)
    , m_threadId(threadId)
    , m_key(0)
    , m_val(0)
    , m_todo(0)
  {
  }
  
  void add(uint8_t byte)
  {
    switch(m_state)
      {
      case StateIdle:
	switch(byte)
	  {
	  case TagStdOut:{
	    //fprintf(stderr, "%08x : Begin stdout\n", m_threadId);
	    m_chars.clear();
	    m_state=StateStdOut;
	    break;
	  }
	  case TagStdErr:{
	    m_chars.clear();
	    m_state=StateStdErr;
	    break;
	  }
	  case TagAssert:{
	    m_state=StateAssert;
	    fprintf(stderr, "ERROR : received assert from thread 0x%x\n", m_threadId);
	    exit(1);
	    break;
	  }
	  case TagKeyVal:{
	    m_state=StateKeyVal_Key;
	    m_key=0;
	    m_val=0;
	    m_todo=4;
	    break;
	  }
	  default:{
	    fprintf(stderr, "ERROR : received control tag %x from thread 0x%x\n", byte, m_threadId);
	    exit(1);
	    break;
	  }
	  }
      case StateStdOut:
	//fprintf(stderr, "%08x : add stdout '%c'\n", m_threadId, (char)byte);
	m_chars.push_back((char)byte);
	if(byte==0){
	fprintf(stdout, "%08x : StdOut : ", m_threadId);
	  fputs(&m_chars[0],stdout);
	  if(m_chars.size()<=1 || m_chars[m_chars.size()-2]!='\n'){
		fputs("\n",stdout);
	  }
	  fflush(stdout);
	  //fprintf(stderr, "%08x : End stdout\n", m_threadId);	  
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
      case StateKeyVal_Key:
	m_key = (m_key>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
	  m_state=StateKeyVal_Val;
	}
	break;
      case StateKeyVal_Val:
	m_val = (m_val>>8) | (uint32_t(byte)<<24);
	m_todo--;
	if(m_todo==0){
	  fprintf(stdout, "KeyVal : %x %x\n", m_key, m_val);
	  m_state=StateIdle;
	}
	break;
      default:
	fprintf(stderr, "ERROR: Unknown state.\n");
	exit(1);
 
      }
  }
};

void protocol(HostLink *link)
{
  std::vector<Protocol> states;

  for(unsigned i=0;i<TinselThreadsPerBoard;i++){
    states.push_back(Protocol(i));
  }

  while(1){
    uint32_t src, val;
    
    uint8_t cmd = link->get(&src, &val);
    uint32_t id = val >> 8;
    uint32_t ch = val & 0xff;
    //    fprintf(stderr, "  cmd=%u, id=%u, ch=%u\n", cmd, id, ch);
    assert(id < TinselThreadsPerBoard);

    states[id].add(ch);
  }
}

