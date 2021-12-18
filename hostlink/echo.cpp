#include "JtagAtlantic.h"
#include <iostream>
#include <unistd.h>
#include <iomanip>

#define min(a,b) ((a) < (b) ? (a) : (b))

// https://github.com/tomverbeure/jtag_uart_example/blob/master/c_client/common.cpp
// DEL SOON
void show_info(JTAGATLANTIC *atlantic) {
    char const *cable;
    int device, instance;
    jtagatlantic_get_info(atlantic, &cable, &device, &instance);
    fprintf(stderr, "Connected to cable '%s', device %d, instance %d\n", cable, device, instance);
}

static const char *err_msgs[] = {
    "No error",
    "Unable to connect to local JTAG server",
    "More than one cable available, provide more specific cable name",
    "Cable not available",
    "Selected cable is not plugged",
    "JTAG not connected to board, or board powered down",
    "Another program is already using the UART",
    "More than one UART available, specify device/instance",
    "No UART matching the specified device/instance",
    "Selected UART is not compatible with this version of the library"
};
void show_err() {
    char const *progname = NULL;
    int err = jtagatlantic_get_error(&progname);
    if(err >= -9 && err <= 0)
        fprintf(stderr, "%s\n", err_msgs[-err]);
    if(progname != NULL && progname[0])
        fprintf(stderr, "progname: '%s'\n", progname);
}

void atlatic_POST() {
  char chain[256];
  int instId = 1;
  JTAGATLANTIC* jtag;
  snprintf(chain, sizeof(chain), "%i", instId);
  jtag = jtagatlantic_open(chain, 0, 0, "hostlink");
  if (jtag == NULL) {
    fprintf(stderr, "Error opening JTAG UART %i\n", instId);
    exit(EXIT_FAILURE);
  }

  if (!jtagatlantic_is_setup_done(jtag)) {
    std::cout << "setup not yet done." << std::endl;
  } else {
    std::cout << "setup done." << std::endl;
  }

  jtagatlantic_wait_open(jtag);

  if (!jtagatlantic_is_setup_done(jtag)) {
    std::cout << "setup not yet done." << std::endl;
  } else {
    std::cout << "setup done." << std::endl;
  }

  show_err();
  show_info(jtag);

  std::cout << "bytes avail to read: " << jtagatlantic_bytes_available(jtag) << std::endl;
  show_err();

  char buf[256];
  int recv, req, avail;
  while (1) {
    avail = jtagatlantic_bytes_available(jtag);
    if (avail) {
      req = min(avail, 255);
      std::cout << "requesting " << req << " bytes. " << std::endl;
      recv = jtagatlantic_read(jtag, buf, req); // queryout
      buf[recv] = '\0';
      std::cout << "recv'd " << recv << " bytes. " << std::endl;
      for (int i=0; i<recv; i++) {
        if (isalnum(buf[i])) std::cout << buf[i];
      }
      std::cout << std::endl;
    }
  }
}

int main(void) {
  atlatic_POST();
}
