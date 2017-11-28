// Compute the average shortest path in a POETS style:
//   * Every node maintains bit vector of nodes that reach it
//   * On each time step, the bit vector is transferred to the neighbours
//   * Any newly-reaching nodes are detected with the current time
//     step representing the distances to those nodes
//   * Time is synchronised in the standard GALS fashion

#ifndef _ASP_H_
#define _ASP_H_

// POETS Synch frontend
#include <Synch.h>

// There are a max of N*32 nodes in the graph
#define N 2

// M*32 nodes are transferred in each message
// (M must divide into N)
#define M 1

struct ASPMessage : PMessage {
  // Offset of reaching vector (see below)
  uint16_t offset;
  // Bit vector of nodes reaching sender
  uint32_t reaching[M];
};

struct ASPDevice {
  // Node id
  uint16_t id;
  // Number of nodes still to reach this device
  uint32_t sum;
  // Bit vector of nodes reaching this device
  uint32_t reaching[N];

  // Called before simulation starts
  inline void init(PDeviceInfo<ASPDevice>* info) {
    // Initialise reaching vector to include only self
    for (uint32_t i = 0; i < N; i++) reaching[i] = 0;
    reaching[id >> 5] = 1 << (id & 0x1f);
  }

  // Called at beginning of each time step
  inline void begin(PDeviceInfo<ASPDevice>* info) {
    for (uint32_t i = 0; i < N; i++) reaching[i] = 0;
  }

  // Called at end of each time step
  inline void end(PDeviceInfo<ASPDevice>* info, ASPDevice* prev) {
    // Copy persistent state
    id = prev->id;
    sum = prev->sum;
    // Add self to reaching vector
    reaching[id >> 5] |= 1 << (id & 0x1f);
    // Update sum
    for (uint32_t i = 0; i < N; i++) {
      uint32_t diff = reaching[i] & ~prev->reaching[i];
      while (diff != 0) {
        sum += info->time+1;
        diff = diff & (diff-1);
      }
    }
  }

  // Called for each message to be sent
  inline void send(PDeviceInfo<ASPDevice>* info, PinId pin,
                   uint32_t chunk, volatile ASPMessage* msg) {
    msg->offset = chunk;
    uint32_t base = chunk * M;
    for (uint32_t i = 0; i < M; i++)
      msg->reaching[i] = reaching[base+i];
  }

  // Called for each message received
  inline void recv(PDeviceInfo<ASPDevice>* info, volatile ASPMessage* msg) {
    uint32_t base = msg->offset * M;
    for (uint32_t i = 0; i < M; i++)
      reaching[base+i] |= msg->reaching[i];
  }

  // Signal completion
  inline bool done(PDeviceInfo<ASPDevice>* info) {
    return info->time == 10;
  }

  // Called on when execution finished
  // (A single message is sent to the host)
  inline void output(PDeviceInfo<ASPDevice>* info, volatile ASPMessage* msg) {
    msg->reaching[0] = sum;
  }
};

#endif
