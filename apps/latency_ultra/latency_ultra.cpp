#include <tinsel.h>

#include "latency.h"

INLINE void tinselSetSendPtr(void *p)
{
  asm volatile("csrrw zero, " CSR_SEND_PTR ", %0" : : "r"(p) : "memory");
}


INLINE void tinselSendToThread0(uint32_t mboxDest)      // Destination mailbox
{
  asm volatile("csrrw zero, " CSR_SEND_DEST ", %0" : : "r"(mboxDest));
  // Opcode: 0000000 rs2 rs1 000 rd 0001000, with rd=0, rs1=x10, rs2=x11
  asm volatile(
    "li x10, 0\n"
    "li x11, 1\n"
    ".word 0x00b50008\n" : :  : "x10", "x11");
}

INLINE void tinselSendToThread0v2(uint32_t mboxDest)      // Destination mailbox
{
  asm volatile("csrrw zero, " CSR_SEND_DEST ", %0" : : "r"(mboxDest));
  // Opcode: 0000000 rs2 rs1 000 rd 0001000, with rd=0, rs1=x10, rs2=x11
  asm volatile(
    "li x11, 1\n"
    ".word 0x00b00008\n" : :  : "x11");
}

INLINE void tinselWaitForRecvReadyThenSendToThread0v2(uint32_t mboxDest)      // Destination mailbox
{
  asm volatile("csrrw zero, " CSR_SEND_DEST ", %0" : : "r"(mboxDest));
  // Opcode: 0000000 rs2 rs1 000 rd 0001000, with rd=0, rs1=x10, rs2=x11
  asm volatile(
    "li x11, 1\n"
    "csrrw zero, " CSR_WAIT_UNTIL ", 2\n"
    ".word 0x00b00008\n" : :  : "x11");
}


int main()
{
  // Get thread id
  int me = tinselId();

  // Get host id
  int host = tinselHostId();

  // Get pointer to mailbox message send slot
  auto msgOut =  (volatile job_msg_t*)tinselSendSlot();
  msgOut->dest_mbox = 0;
  msgOut->source_mbox = me>>TinselLogThreadsPerMailbox;
  msgOut->reps = 0;
  msgOut->cycle_count = 0;

  tinselSetLen(0); // Single flit by default

  // We only ever send from here
  tinselSetSendPtr((void*)msgOut);

  while (1) {
    // This avoids us checking again while on the fast return path
    tinselWaitUntil(TINSEL_CAN_SEND);
    // Post: we can definitely send

    tinselWaitUntil(TINSEL_CAN_RECV);
    volatile job_msg_t* msg = (volatile job_msg_t*) tinselRecv();

    if(msg->reps==0){
      // Pre: We already know that we can send
      tinselSendToThread0v2(msg->source_mbox);
      //tinselMulticast(msg->source_mbox, 0, 1, msgOut);
      tinselFree(msg);
    }else{
      // Pre: we know we can send
      unsigned reps=msg->reps-1;
      unsigned dest_mbox=msg->dest_mbox;
      tinselFree(msg);
      
      msgOut->dest_mbox = dest_mbox;
      msgOut->reps = 0;

      msg=0;
      uint32_t begin_cycle = tinselCycleCount();
      tinselSendToThread0v2(dest_mbox);
      for(unsigned i=0; i<reps; i++){
        // Pre: we know we can send
        tinselWaitUntil(TINSEL_CAN_RECV);
        // Pre: we can both send and receive

        tinselSendToThread0v2(dest_mbox);
        //tinselMulticast(dest_mbox, 0, 1, msgOut);
        // We can still send
        msg = (volatile job_msg_t*) tinselRecv();
        tinselFree(msg);
        // We dont know if we can send or receive

        tinselWaitUntil(TINSEL_CAN_SEND);
        // Post: we know we can send
      }
      tinselWaitUntil(TINSEL_CAN_RECV);
      uint32_t end_cycle = tinselCycleCount();

      msg = (volatile job_msg_t*) tinselRecv();
      tinselFree(msg);

      msgOut->dest_mbox = host;
      msgOut->source_mbox = me>>TinselLogThreadsPerMailbox;
      msgOut->reps = reps+1;
      msgOut->cycle_count = end_cycle - begin_cycle;

      tinselSetLen(TinselMaxFlitsPerMsg-1);
      tinselSend(host, msgOut);
      tinselSetLen(0);
    }
  }

  return 0;
}

