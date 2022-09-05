// SPDX-License-Identifier: BSD-2-Clause
// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>
#include <stdarg.h>

// #include "writeapp.h"

// See "boot.h" for further details of the supported boot commands.

int putchar(int c)
{
  while (tinselUartTryPut(c) == 0);
  return c;
}

INLINE int puts(const char* s)
{
  int count = 0;
  while (*s) { putchar(*s); s++; count++; }
  return count;
}

int puthex(unsigned x)
{
  int count = 0;

  for (count = 0; count < 8; count++) {
    unsigned nibble = x >> 28;
    putchar(nibble > 9 ? ('a'-10)+nibble : '0'+nibble);
    x = x << 4;
  }

  return 8;
}



float compute() {
  // do a benchmark workload
  volatile float y = 0.0;
  for (int i=0; i<10000; i++) {
    for (float x=0; x<50; x=x+1.0) {
      x = x+x;
      y = x*x;
    }
  }
  // y = y + 5.0;
  return y;
}

// Main
int main()
{
  // Global id of this thread
  // Send char to all Threads

  uint32_t me = tinselId();

  // Core-local thread id
  uint32_t threadId = me & ((1 << TinselLogThreadsPerCore) - 1);

  // Host id
  uint32_t hostId = tinselHostId();
  tinselSetLen(0);

  if (threadId == 0) {
    // Use one flit per message
    tinselSetLen(0);
    while ((tinselUartTryGet() & 0x100) == 0);
    for (int t=1; t<TinselThreadsPerCore; t++) {
      tinselCreateThread(t);
    }
  }

  // putchar('f');
  // int (*appMain)() = (int (*)()) (TinselMaxBootImageBytes);
  // putchar('g');
  // appMain();

  // putchar('h');
  // putchar('e');
  // putchar('l');
  // putchar('l');
  // putchar('o');
  // putchar(' ');
  // puthex(me);
  // putchar('\n');

  char* sendbuf = tinselSendSlot();
  void* recvbuf;

  if (me == 0) {
    // thread 0: time the entire process.
    tinselPerfCountReset();
    tinselPerfCountStart();
    for (int c=1; c<TinselCoresPerBoard*TinselThreadsPerCore; c++) {
      tinselWaitUntil(TINSEL_CAN_SEND);
      tinselSend(c, sendbuf);
    }
    compute();
    for (int c=1; c<TinselCoresPerBoard*TinselThreadsPerCore; c++) {
      tinselWaitUntil(TINSEL_CAN_RECV);
      recvbuf = tinselRecv();
      tinselFree(recvbuf);
    }
    tinselPerfCountStop();
    puthex(me);
    putchar(':');
    puthex(tinselCycleCount());
    putchar('\n');
  } else {
    // worker thread; wait to be called, and do compute()
    tinselWaitUntil(TINSEL_CAN_RECV);
    recvbuf = tinselRecv();
    tinselFree(recvbuf);
    compute();
    tinselWaitUntil(TINSEL_CAN_SEND);
    tinselSend(0, sendbuf);
  }

  // Restart boot loader
  if (threadId != 0) tinselKillThread();
  asm volatile("jr zero");

  // Unreachable
  return 0;
}






// APP
