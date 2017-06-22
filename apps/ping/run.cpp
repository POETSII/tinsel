#include <HostLink.h>

int main()
{
  HostLink hostLink;

//uint32_t flit[4];
//hostLink.recv(flit);

  printf("Booting\n");

  hostLink.boot("code.v", "data.v");

  printf("Starting\n");
  hostLink.go();


/*
  printf("Waiting for response\n");
  uint32_t flit[4];
  hostLink.recv(flit);
  printf("Got response %x\n", flit[0]);
*/

  while(1);

  return 0;
}
