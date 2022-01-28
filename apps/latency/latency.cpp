#include <tinsel.h>

#include "latency.h"

int main()
{
  // Get thread id
  int me = tinselId();

  // Get host id
  int host = tinselHostId();

  // Get pointer to mailbox message send slot
  auto msgOut =  (volatile job_msg_t*)tinselSendSlot();
  msgOut->dest = 0;
  msgOut->source = me;
  msgOut->reps = 0;
  msgOut->cycle_count = 0;

  tinselSetLen(0); // Single flit by default

  while (1) {
    // This avoids us checking again while on the fast return path
    tinselWaitUntil(TINSEL_CAN_SEND);
    // Post: we can definitely send

    tinselWaitUntil(TINSEL_CAN_RECV);
    volatile job_msg_t* msg = (volatile job_msg_t*) tinselRecv();

    if(msg->reps==0){
      // Pre: We already know that we can send
      msgOut->dest = msg->source;
      
      tinselSend(msg->dest, msgOut);
      tinselFree(msg);
    }else{
      // Pre: we know we can send
      unsigned reps=msg->reps;
      unsigned dest=msg->dest;
      tinselFree(msg);
      
      msgOut->dest = dest;
      msgOut->reps = 0;

      msg=0;
      uint32_t begin_cycle = tinselCycleCount();
      for(unsigned i=0; i<reps; i++){
        // Pre: we know we can send

        tinselSend(dest, msgOut);
        if(i!=0){
          msg = (volatile job_msg_t*) tinselRecv();
          tinselFree(msg);
        }
        tinselWaitUntil(TINSEL_CAN_SEND);
        // Post: we know we can send

        tinselWaitUntil(TINSEL_CAN_RECV);
      }
      uint32_t end_cycle = tinselCycleCount();

      msg = (volatile job_msg_t*) tinselRecv();
      tinselFree(msg);

      msgOut->dest = host;
      msgOut->source = me;
      msgOut->reps = reps;
      msgOut->cycle_count = end_cycle - begin_cycle;

      tinselSetLen(TinselMaxFlitsPerMsg-1);
      tinselSend(host, msgOut);
      tinselSetLen(0);
    }
  }

  return 0;
}

