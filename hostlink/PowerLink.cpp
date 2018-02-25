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

// Open a power link
int powerInit(char* dev)
{
  int fd = open(dev, O_RDWR | O_NOCTTY);
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

// Send a command over a power link
void powerPutCmd(int fd, char* cmd, char* resp, int respSize)
{
  // Send command
  char* ptr = cmd;
  while (*ptr) {
    int n = write(fd, ptr, 1);
    if (n == -1) {
      perror("write on power tty");
      exit(EXIT_FAILURE);
    }
    if (n == 1) ptr++;
  }
  // Receive response
  int got = 0;
  while (got < (respSize-1)) {
    char c;
    int n = read(fd, &c, 1);
    if (n == -1) {
      perror("read on power tty");
      exit(EXIT_FAILURE);
    }
    if (n == 1) {
      if (c == '!') {
        resp[got] = '\0';
        return;
      }
      resp[got++] = c;
    }
  }
  resp[got] = '\0';
}

// Enable power to all worker FPGAs
void powerEnable(int enable)
{
  // Determine all the power links
  char line[256];
  FILE* fp = popen("ls /dev/serial/by-id/usb-Cypress*", "r");
  if (!fp) {
    fprintf(stderr, "Power-enable failed");
    exit(EXIT_FAILURE);
  }
  // For each power link
  while (1) {
    if (fgets(line, sizeof(line), fp) == NULL) return;
    if (feof(fp)) return;
    // Trim the new line
    char* ptr = line;
    while (*ptr) { if (*ptr == '\n') *ptr = '\0'; ptr++; }
    // Open link
    int fd = powerInit(line);
    // Send command
    char resp[256];
    if (enable)
      powerPutCmd(fd, (char*) "p=1.", resp, sizeof(resp));
    else
      powerPutCmd(fd, (char*) "p=0.", resp, sizeof(resp));
    // Close link
    close(fd);
  }
}

// Disable then enable power to all worker FPGAs
void powerReset()
{
  powerEnable(0);
  sleep(3);
  powerEnable(1);
}
