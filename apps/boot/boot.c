// SPDX-License-Identifier: BSD-2-Clause
// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>



// See "boot.h" for further details of the supported boot commands.

INLINE void sendc(int c) {
  // tinselUartTryPut(x);
  while (tinselUartTryPut(c) == 0);
}

INLINE int puts(const char* s)
{
  int count = 0;
  while (*s) { sendc(*s); s++; count++; }
  return count;
}

INLINE int puthex(unsigned x)
{
  int count = 0;
  sendc( '0' ); sendc( 'x' );

  for (count = 0; count < 8; count++) {
    unsigned nibble = x >> 28;
    sendc(nibble > 9 ? ('a'-10)+nibble : '0'+nibble);
    x = x << 4;
  }

  return 8;
}

// INLINE void itoa(uint32_t x, char* s) {
//   uint8_t w;
//   for (int i=0; i<4; i++) {
//     w = ((uint8_t *)(&x))[3-i];
//     s[2*i] = (w&0xF)+48;
//     s[2*i+1] = (w>>4 & 0xF)+48;
//   }
// }

// char* leader = "message from core/thrd id 0x";
// char* recv = " recv byte 0x";

// INLINE void pingpong(uint32_t me) {
//   uint32_t c = 0;
//   while (c >> 7 <= 0) { c = tinselUartTryGet(); }
//   sendc( 'i' ); sendc( 'd' ); sendc( ' ' ); // sendc( '0' ); sendc( 'x' );
//   puthex( me );
//   // puts(recv);
//   //sendc( ' ' );
//   sendc( ' ' ); sendc( 'v' ); sendc( ' ' ); // sendc( 'e' ); sendc( 'c' ); sendc( 'v' ); sendc( 'd' ); sendc( ' ' ); sendc( '0' ); sendc( 'x' );
//   puthex( c );
//   sendc( '\n' );
//   sendc( '\0' );
//   for (uint8_t i=0; i<100; i++) { asm volatile("ADDI x0, x0, 0"); };
// }

// INLINE void printbase(uint32_t me) {
//   uint32_t c = 0;
//   while (c >> 7 <= 0) { c = tinselUartTryGet(); }
//   sendc( 'i' ); sendc( 'd' ); sendc( ' ' ); // sendc( '0' ); sendc( 'x' );
//   puthex( me );
//   sendc( ' ' ); sendc( 't' ); sendc( 'r' ); sendc( 'd' ); sendc( 'b' ); sendc( ' ' ); // sendc( 'e' ); sendc( 'c' ); sendc( 'v' ); sendc( 'd' ); sendc( ' ' ); sendc( '0' ); sendc( 'x' );
//   puthex( tinselHeapBaseGeneric(me) );
//   sendc( '\n' ); sendc( '\0' );
// }

// INLINE void printaddrmsg(uint32_t me) {
//   uint32_t c = 0;
//   while (c >> 7 <= 0) { c = tinselUartTryGet(); }
//   sendc( 'i' ); sendc( 'd' ); sendc( ' ' ); // sendc( '0' ); sendc( 'x' );
//   puthex( me );
//   sendc( ' ' ); sendc( 'm' ); sendc( 's' ); sendc( 'g' ); sendc( '&' ); sendc( ' ' ); // sendc( 'e' ); sendc( 'c' ); sendc( 'v' ); sendc( 'd' ); sendc( ' ' ); sendc( '0' ); sendc( 'x' );
//   puthex( &msg );
//   sendc( '\n' ); sendc( '\0' );
// }

// INLINE void printstackaddr(uint32_t me) {
//   uint32_t c = 0;
//   while (c >> 7 <= 0) { c = tinselUartTryGet(); }
//   sendc( 'i' ); sendc( 'd' ); sendc( ' ' ); // sendc( '0' ); sendc( 'x' );
//   puthex( me );
//   sendc( ' ' ); sendc( 'c' ); sendc( '&' ); sendc( ' ' ); // sendc( 'e' ); sendc( 'c' ); sendc( 'v' ); sendc( 'd' ); sendc( ' ' ); sendc( '0' ); sendc( 'x' );
//   puthex( &c );
//   sendc( '\n' ); sendc( '\0' );
// }
//
// void recursive(uint32_t me, int count) {
//   sendc( 'c' ); sendc( 'o' ); sendc( 'u' ); sendc( 'n' ); sendc( 't' ); sendc( ' ' ); // sendc( '0' ); sendc( 'x' );
//   puthex( count );
//   sendc( '\n' );
//   recursive(me, count+1);
// }

typedef union intfloat {
  uint32_t i;
  float f;
} intfloat_t;

void testfp() {
  intfloat_t a, b, c;
  a.i = 0; b.i = 0; c.i = 0;
  for (; a.i < 1<<31; a.i++) {
    for (; b.i < 1<<31; b.i++) {
      c.f = (a.f + b.f);
      puthex( a.i ); sendc( '+' ); puthex( b.i ); sendc( '=' ); puthex( c.i ); sendc( '\n' );
    }
  }
}

// Main
int main()
{
  // Global id of this thread
  uint32_t me = tinselId();

  // Core-local thread id
  uint32_t threadId = me & ((1 << TinselLogThreadsPerCore) - 1);
  // uint32_t coreId = me >> (TinselLogThreadsPerCore);


  // Host id
  // uint32_t hostId = tinselHostId();

  if (threadId == 0) {
    // State
    // uint32_t addrReg = 0;  // Address register
    // uint32_t lastDataStoreAddr = 0;

    // Get mailbox message slot for sending
    // volatile uint32_t* msgOut = tinselSendSlot();
    // Command loop

    for (;;) {
      uint32_t c = 0;
      while (c >> 7 <= 0) { c = tinselUartTryGet(); }

      testfp();

    }
  }


  // Restart boot loader
  if (threadId != 0) tinselKillThread();
  asm volatile("jr zero");

  // Unreachable
  return 0;
}


// while (1) {};





// uint8_t c_old = 0;
// // //  // block until we recv a message
// // c = tinselUartTryGet();
// if (me == 0) {
//   while (1) {
//
//     c = tinselUartTryGet();
//     if (c != c_old) {
//       sendc( '0' );
//       sendc( 'x' );
//       puthex( c );
//       sendc( '\n' );
//       c = c_old;
//     }
//   }
// }

// pregel, vertex-centric - model should include scalability
// not the full costom design, it's a bit unrelaistic


// do { c = tinselUartTryGet(); } while (c>>7 == 0); // block until we recv a message
// if (me == 0) {
  // while (1) { puts("hello\0"); };
  // while (1) { sendc(c); c++; };

// }
// puts(id);
// if (me == 0) {
//   puts(id);
//   // puthex(me);
// }
// sendc(c);
// for (int i=0; i<4; i++) {
//   tinselUartTryPut(((char *)(&me))[i]);
// }

// while (1) {}; // block


// if (me == 0) {
//   // while (1) {
//   //   sendc(count);
//   //   count++;
//   // }
//   while (1) {
//     for (int i=0; i<16; i++) {
//       sendc(id[i]);
//     }
//   }
// }

// // Receive message
// tinselWaitUntil(TINSEL_CAN_RECV);
// volatile BootReq* msgIn = tinselRecv();
//
// // Command dispatch
// // (We avoid using a switch statement here so that the compiler
// // doesn't generate a data section)
// uint8_t cmd = msgIn->cmd;
// if (cmd == WriteInstrCmd) {
//   // Write instructions to instruction memory
//   int n = msgIn->numArgs;
//   for (int i = 0; i < n; i++) {
//     tinselWriteInstr(addrReg, msgIn->args[i]);
//     addrReg += 4;
//   }
// }
// else if (cmd == StoreCmd) {
//   // Store words to data memory
//   int n = msgIn->numArgs;
//   for (int i = 0; i < n; i++) {
//     uint32_t* ptr = (uint32_t*) addrReg;
//     *ptr = msgIn->args[i];
//     lastDataStoreAddr = addrReg;
//     addrReg += 4;
//   }
// }
// else if (cmd == LoadCmd) {
//   // Load words from data memory
//   int n = msgIn->args[0];
//   while (n > 0) {
//     int m = n > 4 ? 4 : n;
//     tinselWaitUntil(TINSEL_CAN_SEND);
//     for (int i = 0; i < m; i++) {
//       uint32_t* ptr = (uint32_t*) addrReg;
//       msgOut[i] = *ptr;
//       addrReg += 4;
//       n--;
//     }
//     tinselSend(hostId, msgOut);
//   }
// }
// else if (cmd == SetAddrCmd) {
//   // Set address register
//   addrReg = msgIn->args[0];
// }
// else if (cmd == StartCmd) {
//   // Cache flush
//   tinselCacheFlush();
//   // Wait until lines written back, by issuing a load
//   if (lastDataStoreAddr != 0) {
//     volatile uint32_t* ptr = (uint32_t*) lastDataStoreAddr; ptr[0];
//   }
//   // Send response
//   tinselWaitUntil(TINSEL_CAN_SEND);
//   msgOut[0] = tinselId();
//   tinselSend(hostId, msgOut);
//   // Wait for trigger
//   while ((tinselUartTryGet() & 0x100) == 0);
//   // Start remaining threads
//   int numThreads = msgIn->args[0];
//   for (int i = 0; i < numThreads; i++)
//     tinselCreateThread(i+1);
//   tinselFree(msgIn);
//   break;
// }
// tinselFree(msgIn);
