// SPDX-License-Identifier: BSD-2-Clause
#include "ping.h"

#include <HostLink.h>
#include <POLite.h>
#include <EdgeList.h>
#include <assert.h>
#include <sys/time.h>
#include <config.h>

int main(int argc, char**argv)
{
  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<PingDevice, PingState, None, PingMessage> graph;

  // Create single ping device
  PDeviceId id = graph.newDevice();

  // Prepare mapping from graph to hardware
  graph.map();

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();

  printf("Ping started\n");

  // Consume performance stats
  //politeSaveStats(&hostLink, "stats.txt");

  int test = 0;
  int deviceAddr = graph.toDeviceAddr[id];
  printf("deviceAddr = %d\n", deviceAddr);
  while (test < 100) {
    // Send ping
    PMessage<None, PingMessage> sendMsg;
    sendMsg.payload.test = test;
    hostLink.send(deviceAddr, 1, &sendMsg);
    printf("Sent %d to device\n", sendMsg.payload.test);

    // Receive pong
    PMessage<None, PingMessage> recvMsg;
    hostLink.recvMsg(&recvMsg, sizeof(recvMsg));
    printf("Received %d from device\n", recvMsg.payload.test);

    test++;
  }

  return 0;
}
