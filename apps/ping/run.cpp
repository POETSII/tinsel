#include <HostLink.h>

int main()
{
  HostLink hostLink;

  printf("Booting\n");
  hostLink.boot("code.v", "data.v");

  printf("Starting\n");
  hostLink.go();

  printf("Sending ping\n");
  uint32_t ping[4];
  hostLink.send(0, 1, ping);

  printf("Waiting for response\n");
  hostLink.recv(ping);
  printf("Got response %x\n", ping[0]);

  while(1);

  return 0;
}
