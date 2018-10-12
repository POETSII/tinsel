#include <HostLink.h>

int main()
{
  HostLink hostLink;

  printf("Booting\n");
  hostLink.boot("code.v", "data.v");

  printf("Starting\n");
  hostLink.go();

  printf("Waiting for response\n");
  uint32_t resp[4];
  hostLink.recv(resp);
  printf("Got response\n");

  return 0;
}
