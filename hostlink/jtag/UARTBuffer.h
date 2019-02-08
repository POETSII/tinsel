#ifndef _UART_BUFFER_H_
#define _UART_BUFFER_H_

#include <stdint.h>
#include <unistd.h>
#include "UART.h"
#include "Queue.h"

#define UART_BUFFER_SIZE 4096

// Buffered I/O over JTAG UART
struct UARTBuffer {
  UART* uart;
  Queue<uint8_t>* in;
  Queue<uint8_t>* out;

  UARTBuffer() {
    uart = new UART;
    in = new Queue<uint8_t> (UART_BUFFER_SIZE);
    out = new Queue<uint8_t> (UART_BUFFER_SIZE);
  }

  // Serve the UART, i.e. fill and release I/O buffers
  inline void serve() {
    uint8_t buffer[128];
    bool progress = true;
    while (progress) {
      progress = false;
      // Queue -> UART
      if (out->size > 0) {
        int bytes = out->size < sizeof(buffer) ? out->size : sizeof(buffer);
        for (int i = 0; i < bytes; i++) buffer[i] = out->index(i);
        int n = uart->write((char*) buffer, bytes);
        if (n > 0) {
          out->drop(n);
          progress = true;
        }
      }
      // UART -> QUEUE
      if (in->space() > sizeof(buffer)) {
        int n = uart->read((char*) buffer, sizeof(buffer));
        for (int i = 0; i < n; i++) in->enq(buffer[i]);
        if (n > 0) progress = true;
      }
    }
  }

  inline bool canPut(int n) {
    return out->space() >= n;
  };
  inline void put(char byte) {
    out->enq(byte);
  }

  inline bool canGet(int n) {
    return in->size >= n;
  }
  inline uint8_t get() {
    return in->deq();
  }
  inline uint8_t peek() {
    return in->first();
  }
  inline uint8_t peekAt(int i) {
    return in->index(i);
  }

  void flush() {
    while (out->size > 0) {
      serve();
      usleep(100);
    }
  }

  ~UARTBuffer() {
    delete [] in;
    delete [] out;
    delete uart;
  }
};

#endif
