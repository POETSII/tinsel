// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>

// See "boot.h" for details of the protocol used between the boot
// loader (running on tinsel threads) and the the host PC.

int main()
{
  // Ids for this thread and the host PC
  uint32_t me = get_my_id();
  uint32_t host = get_host_id();

  // State
  uint32_t reqCount = 0; // Count of requests received
  uint32_t checksum = 0; // Checksum of requests received
  uint32_t startAddr;    // Start address of loaded code

  // Pointers into scratchpad for incoming and outgoing messages
  volatile BootMsg* req       = mailbox(0); // Incoming
  volatile BootMsg* resp      = mailbox(1); // Outgoing
  volatile BootMsg* broadcast = mailbox(2); // Outgoing

  // Set source field of response
  resp->src = me;

  // Allocate space for incoming request
  mb_alloc(req);

  // Event loop
  for (;;) {
    // Receive an incoming message
    mb_wait_until(CAN_RECV);
    volatile BootMsg* msg = mb_recv();

    // Copy the message fields and reallocate the message slot
    uint32_t src  = msg->src;
    uint32_t cmd  = msg->cmd;
    uint32_t data = msg->data;
    uint32_t addr = msg->addr;
    // Only reallocate if the boot loader isn't finished
    if (cmd != StartReq) mb_alloc(msg);

    // Update request-count and checksum
    reqCount++;
    checksum += src + cmd + addr + data;

    // Command dispatch
    switch (cmd & BootCmdMask) {
      // Send the request-count
      case GetCountReq: {
        mb_wait_until(CAN_SEND);
        resp->cmd = CountResp;
        resp->data = reqCount;
        mb_send(host, resp);
        // Reset count
        reqCount = 0;
        break;
      }
      // Send the checksum
      case GetChecksumReq: {
        mb_wait_until(CAN_SEND);
        resp->cmd = ChecksumResp;
        resp->data = checksum;
        mb_send(host, resp);
        break;
      }
      // Write to instruction memory
      case WriteInstrReq: {
        write_instr(addr, data);
        break;
      }
      // Store word to memory
      case StoreReq: {
        volatile uint32_t* ptr = (uint32_t*) addr;
        *ptr = data;
        break;
      }
      // Load word from memory
      case LoadReq: {
        volatile uint32_t* ptr = (uint32_t*) addr;
        mb_wait_until(CAN_SEND);
        resp->cmd = LoadResp;
        resp->data = *ptr;
        mb_send(host, resp);
        break;
      }
      // Cache flush
      case CacheFlushReq: {
        flush();
        break;
      }
    }

    // Do we need to broadcast the request to other threads on same board?
    if (cmd & BroadcastReq) {
      mb_wait_until(CAN_SEND);
      broadcast->src  = src;
      broadcast->cmd  = cmd & BootCmdMask; // Remove the broadcast bit
      broadcast->addr = addr;
      broadcast->data = data;
      for (uint32_t dest = 0; dest < ThreadsPerBoard; dest++)
        if (dest != me) {
          mb_wait_until(CAN_SEND);
          mb_send(dest, broadcast);
        }
    }

    // Exit event loop
    if (cmd == StartReq) {
      startAddr = addr;
      break;
    }
  }

  // Jump to start address
  asm volatile ("jr %0" : : "r"(startAddr));

  // Unreachable
  return 0;
}
