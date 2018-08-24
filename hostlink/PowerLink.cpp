#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include "PowerLink.h"

#define DEFAULT_TINSEL_ROOT "/local/tinsel"

// Enable power to all worker FPGAs
void powerEnable(int enable)
{
  char* root = getenv("TINSEL_ROOT");
  if (root == NULL) {
    root = (char*) DEFAULT_TINSEL_ROOT;
  }
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "%s/bin/fpga-power.sh %s", root,
    enable ? "on" : "off");
  if (system(cmd) < 0) {
    fprintf(stderr, "Can't run '%s'\n", cmd);
    exit(EXIT_FAILURE);
  }
}

// Wait for FPGAs to be detected after powerup
void waitForFPGAs(int numFPGAs)
{
  char* root = getenv("TINSEL_ROOT");
  if (root == NULL) {
    root = (char*) DEFAULT_TINSEL_ROOT;
  }
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "%s/bin/wait-for-fpgas.sh %d", root, numFPGAs);
  if (system(cmd) < 0) {
    fprintf(stderr, "Can't run '%s'\n", cmd);
    exit(EXIT_FAILURE);
  }
}
