#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PowerLink.h"

void usage()
{
  printf("tinsel-power (on|off)\n");
  exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
  if (argc < 2) usage();

  if (! strcmp(argv[1], "on")) {
    powerEnable(1);
  }
  else if (! strcmp(argv[1], "off")) {
    powerEnable(0);
  }
  else {
    usage();
  }

  return 0;
}
