#include <HostLink.h>

int main()
{
  HostLink hostLink;

  printf("Booting\n");
  hostLink.boot("code.v", "data.v");

  printf("Starting\n");
  hostLink.go();

  printf("Sending ping\n");
  uint32_t ping[1 << TinselLogWordsPerMsg];
  ping[0] = 100;
  hostLink.send(0, ping);

  printf("Waiting for response\n");
  hostLink.recv(ping);
  printf("Got response %x\n", ping[0]);

  return 0;
}
