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
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <SocketUtils.h>
#include <DebugLink.h>

// If the fan tach is below this threshold then we report an error
#define FAN_TACH_THRESHOLD 2500

// Email address to use for error report
#define EMAIL_ADDR "mn416@cam.ac.uk"

// SMTP server for email
#define SMTP_SERVER "ppsw.cam.ac.uk"

// Open connect to FPGA power board
int powerBoardInit(char* dev)
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

// Send a command to power board, and receive response
int powerBoardPutCmd(int fd, char* cmd, char* resp, int respSize)
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

// Send an email reporting low fan tach
bool reportViaEmail(int fpgaNum, int fanTach)
{
  // Get host name
  char hostname[256];
  hostname[0] = '\0';
  gethostname(hostname, sizeof(hostname));
  hostname[sizeof(hostname)-1] = '\0';
  // Send email
  FILE* fp = popen("msmtp -f " EMAIL_ADDR
                   " --host=" SMTP_SERVER
                   " " EMAIL_ADDR, "w");
  if (fp == NULL) return false;
  fprintf(fp, "Subject: Fan tach check failed on %s\n", hostname);
  fprintf(fp, "FPGA %i had tach of %i\n", fpgaNum, fanTach);
  fclose(fp);
  return true;
}

// Helper: blocking receive of a BoardCtrlPkt
void getPacket(int conn, BoardCtrlPkt* pkt)
{
  int got = 0;
  char* buf = (char*) pkt;
  int numBytes = sizeof(BoardCtrlPkt);
  while (numBytes > 0) {
    int ret = recv(conn, &buf[got], numBytes, 0);
    if (ret < 0) {
      fprintf(stderr, "Connection to box boardctrld failed\n");
      fprintf(stderr, "(box may already be in use)\n");
      exit(EXIT_FAILURE);
    }
    else {
      got += ret;
      numBytes -= ret;
    }
  }
}

// Turn on power boards
int powerup()
{
  // Connect to board control daemon, which powers up FPGAs for us
  int conn = socketConnectTCP("localhost", 10101);
  // Wait for ready packet
  BoardCtrlPkt pkt;
  getPacket(conn, &pkt);
  assert(pkt.payload[0] == DEBUGLINK_READY);
  return conn;
}

int main()
{
  char buffer[1024];
  int conn = powerup();
  sleep(5); // More time for fans get up to speed
  for (int i = 0; i < 6; i++) {
    // TODO: remove assumption on how udev assigns devices to names
    snprintf(buffer, sizeof(buffer), "/dev/ttyACM%d", i);
    int fd = powerBoardInit(buffer);
    powerBoardPutCmd(fd, (char*) "f?", buffer, sizeof(buffer));
    int tach = atoi(buffer);
    printf("Device %i: tach=%i\n", i, tach);
    close(fd);
    if (tach < FAN_TACH_THRESHOLD) {
      printf("Tach too low: reporting error\n");
      reportViaEmail(i, tach);
      close(conn);
      return 0;
    }
  }
  close(conn);
  return 0;
}
