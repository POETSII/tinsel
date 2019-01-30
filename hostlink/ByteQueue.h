#ifndef _BYTE_QUEUE_H_
#define _BYTE_QUEUE_H_

#include <stdint.h>

// Simple queue of bytes
struct ByteQueue {
  int capacity;
  int front, back;
  int size;
  uint8_t* data;

  Queue(int n) {
    capacity = n+1;
    data = new uint8_t [capacity];
  }

  bool canEnq() {
    return ((back+1)%capacity) != front;
  }

  void enq(uint8_t byte) {
    data[back] = byte;
    back++;
    size++;
  }

  bool canDeq() {
    return front != back;
  }

  uint8_t deq() {
    byte = data[front];
    front++;
    size--;
    return byte;
  }

  int space() {
    return (capacity-1-size);
  }

  ~Queue() {
    delete [] data;
  }
};

#endif
