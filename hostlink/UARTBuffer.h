#include "UART.h"
#include "ByteQueue.h"

// I/O buffer for sending and receiving packets
struct UARTBuffer {
  UART* uart;
  ByteQueue* inQueue;
  ByteQueue* outQueue;

// TODO

  UARTBuffer(UART* u, int n) {
  }

  void progress() {
    if (outBack > outFront) {
      int ret = uart->write(&bytesOut[outFront], outBack - outFront);
    }

    if (nbytesOut > 0) {
      int ret = uart->write(&bytesOut[nbytesOut], nbytesOut);
      if (ret > 0) {
        bytesOut += ret;
        if bytesOut
      }
    }
  };

  bool canPut() {
    if (nbytesOut < size) return true;

    uart->write();
  };
  void put(char byte) {
    bytesOut[nbytesOut] = byte;
    nbytesOut++
  }

  bool canGet() { return bytesIn == sizeof(BoardCtrlPkt); }
  void get() {
  }

  ~PktBuffer() {
    delete [] bytesIn;
    delete [] bytesOut;
  }
};
