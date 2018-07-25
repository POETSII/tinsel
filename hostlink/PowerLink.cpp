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

// Open a power link
int powerInit(char* dev)
{
  int fd = open(dev, O_RDWR | O_NONBLOCK | O_NOCTTY);
  if (fd == -1) {
    perror("open");
    exit(EXIT_FAILURE);
  }

  // Get attributes
  struct termios tty;
  if (tcgetattr(fd, &tty) == -1) {
    fprintf(stderr, "Can't get termios attributes on '%s'\n", dev);
    exit(EXIT_FAILURE);
  }

  // Baud rate
  cfsetospeed(&tty, B115200);
  cfsetispeed(&tty, B115200);
  // Raw mode
  cfmakeraw(&tty);
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 0;
  // No parity
  tty.c_cflag &= ~(PARENB | PARODD | CMSPAR);
  // 8 data bits
  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
  // 1 stop bit
  tty.c_cflag &= ~CSTOPB;
  // No flow control
  tty.c_cflag &= ~(CRTSCTS);
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  // Local
  tty.c_cflag |= CLOCAL;
  // HUP on close
  //tty.c_cflag |= HUPCL;
  tty.c_cflag &= ~HUPCL;

  // Set attributes
  if (tcsetattr(fd, TCSANOW, &tty) == -1) {
    fprintf(stderr, "Can't set termios attributes on '%s'\n", dev);
    exit(EXIT_FAILURE);
  }

  return fd;
}

// Reset the PowerLink PSoCs
// (Starting from link with given index)
void powerResetPSoCs(int index)
{
  char* root = getenv("TINSEL_ROOT");
  if (root == NULL) {
    root = (char*) DEFAULT_TINSEL_ROOT;
  }
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "%s/bin/reset-psocs.sh %d", root, index);
  if (system(cmd) < 0) {
    fprintf(stderr, "Can't run '%s'\n", cmd);
    exit(EXIT_FAILURE);
  }
}

// Send a command over a power link
int powerPutCmd(int fd, char* cmd, char* resp, int respSize)
{
  // For 'select' call
  struct timeval tv;
  fd_set fds;

  // Send command
  char* ptr = cmd;
  while (*ptr) {
    // Initialise descriptor set
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    // Set timeout
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    // Wait until write is possible
    int ret = select(fd+1, NULL, &fds, NULL, &tv);
    if (ret < 0) {
      perror("select() on power link");
      return -1;
    }
    else if (ret == 0) {
      // Timeout elapsed
      //fprintf(stderr, "timeout on power link write\n");
      return -1;
    }
    else {
      // Do the write
      int n = write(fd, ptr, 1);
      if (n == -1) {
        perror("write() on power link");
        return -1;
      }
      if (n == 1) ptr++;
    }
  }
  // Receive response
  int got = 0;
  while (got < (respSize-1)) {
    // Initialise descriptor set
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    // Set timeout
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    // Wait until read is possible
    int ret = select(fd+1, &fds, NULL, NULL, &tv);
    if (ret < 0) {
      perror("select() on power link");
      return -1;
    }
    else if (ret == 0) {
      // Timeout elapsed
      //fprintf(stderr, "timeout on power link read");
      return -1;
    }
    else {
      char c;
      int n = read(fd, &c, 1);
      if (n == -1) {
        perror("read() on power link");
        return -1;
      }
      if (n == 1) {
        if (c == '!') {
          resp[got] = '\0';
          return 0;
        }
        resp[got++] = c;
      }
    }
  }
  resp[got] = '\0';
  return 0;
}

// Send a command over all power links
int powerPutCmdAll(const char* cmd, bool retry)
{
  int retryCount = 0;
  // Retry from here, if necessary
  retry_label:
  // Determine all the power links
  char line[256];
  FILE* fp = popen(
    "ls /dev/serial/by-id/usb-Cypress_Semiconductor_USBUART*", "r");
  if (!fp) {
    fprintf(stderr, "Power-enable failed\n");
    return -1;
  }
  // For each power link
  for (int i = 0; ; i++) {
    if (fgets(line, sizeof(line), fp) == NULL) break;
    if (feof(fp)) break;
    // Trim the new line
    char* ptr = line;
    while (*ptr) { if (*ptr == '\n') *ptr = '\0'; ptr++; }
    // Open link
    int fd = powerInit(line);
    // Send command
    char resp[256];
    int ok = powerPutCmd(fd, (char*) cmd, resp, sizeof(resp));
    // On error, optionally reset PSoCs and retry
    if (ok < 0) {
      if (retry && retryCount < 2) {
        retryCount++;
        powerResetPSoCs(0);
        close(fd);
        fclose(fp);
        goto retry_label;
      }
      else {
        fprintf(stderr, "Temporarily unable to connect to PowerLinks\n");
        return -1;
      }
    }
    // Close link
    close(fd);
  }
  fclose(fp);
  return 0;
}

// Enable power to all worker FPGAs
int powerEnable(int enable)
{
  // Try to talk to power boards and reset them if neccessary
  // (This is a workaround for occasionally unresponsive powers boards)
  int ret = powerPutCmdAll("p?", true);
  if (ret < 0) return -1;
  // Now enable or disable the power
  if (enable)
    return powerPutCmdAll("p=1.", false);
  else
    return powerPutCmdAll("p=0.", false);
}

// Disable then enable power to all worker FPGAs
int powerReset()
{
  int ret = powerEnable(0);
  if (ret < 0) {
    fprintf(stderr, "Failed to reset FPGAs.\n");
    return -1;
  }
  sleep(3);
  ret = powerEnable(1);
  if (ret < 0) {
    fprintf(stderr, "Failed to reset FPGAs.\n");
    return -1;
  }
  return 0;
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
