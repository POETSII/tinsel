#include <HostLink.h>

int main()
{
  HostLink hostLink;

  printf("Booting\n");

  // Load program 0 onto core 0
  hostLink.loadInstrsOntoCore("code_0.v", 0, 0, 0);
  hostLink.loadDataOntoDRAM("data_0.v", 0, 0, 0);

  // Load program 1 onto core 1
  hostLink.loadInstrsOntoCore("code_1.v", 0, 0, 1);
  hostLink.loadDataOntoDRAM("data_1.v", 0, 0, 0);
  // TODO: loadDataViaCore

  // Start cores
  hostLink.startOne(0, 0, 0, 1);
  hostLink.startOne(0, 0, 1, 1);

  // Trigger cores 0 and 1 into execution
  printf("Starting\n");
  hostLink.goOne(0, 0, 0);
  hostLink.goOne(0, 0, 1);

  printf("Sending ping to thread 0\n");
  uint32_t ping[4];
  ping[0] = 100;
  hostLink.send(0, 1, ping);

  printf("Sending ping to thread 16\n");
  hostLink.send(16, 1, ping);

  printf("Waiting for responses\n");
  hostLink.recv(ping);
  printf("Got response %x\n", ping[0]);
  hostLink.recv(ping);
  printf("Got response %x\n", ping[0]);

  return 0;
}
