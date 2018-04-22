// Command-line utilities for UNIX Domain Sockets

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <poll.h>


void usage()
{
  printf("Connect two UNIX Domain Sockets:\n"
         "  udsock join [SOCKET] [SOCKET]\n\n"
         "Connect UNIX Domain Socket to stdin:\n"
         "  udsock in [SOCKET]\n\n"
         "Connect UNIX Domain Socket to stdout:\n"
         "  udsock out [SOCKET]\n\n"
         "Connect UNIX Domain Socket to stdin and stdout:\n"
         "  udsock inout [SOCKET]\n\n");
  exit(EXIT_FAILURE);
}

void join(int out, int in)
{
  char buffer[65536];

  for (;;) {
    // Read from 'in'
    int n = read(in, buffer, sizeof(buffer));
    if (n <= 0) return;
    // Write to 'out'
    int i = 0;
    while (n > 0) {
      int m = write(out, &buffer[i], n);
      if (m <= 0) return;
      n = n-m;
      i = i+m;
    }
  }
}

int main(int argc, char* argv[])
{
  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  int sock1 = -1;
  int sock2 = -1;
  int in    = -1;
  int out   = -1;

  if (argc < 3) usage();

  sock1 = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock1 == -1) {
    perror("socket");
    return -1;
  }

  // Connect to socket
  struct sockaddr_un addr, addr2;
  memset(&addr, 0, sizeof(struct sockaddr_un));
  memset(&addr2, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, argv[2], sizeof(addr.sun_path)-1);
  for (int i = 0; i < strlen(addr.sun_path); i++)
    if (addr.sun_path[i] == '@') addr.sun_path[i] = '\0';
  while (connect(sock1, (struct sockaddr *) &addr,
                   sizeof(struct sockaddr_un)) < 0) sleep(1);

  if (argc == 4) {
    if (!strcmp(argv[1], "join")) {
      sock2 = socket(AF_UNIX, SOCK_STREAM, 0);
      if (sock2 == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
      }

      // Connect to socket
      addr2.sun_family = AF_UNIX;
      strncpy(addr2.sun_path, argv[3], sizeof(addr2.sun_path)-1);
      for (int i = 0; i < strlen(addr2.sun_path); i++)
        if (addr2.sun_path[i] == '@') addr2.sun_path[i] = '\0';
      while (connect(sock2, (struct sockaddr *) &addr2,
                       sizeof(struct sockaddr_un)) < 0) sleep(1);
    }
    else
      usage();
    if (fork() == 0)
      join(sock2, sock1);
    else
      join(sock1, sock2);
  }
  else if (argc == 3) {
    if (!strcmp(argv[1], "in"))
      join(sock1, STDIN_FILENO);
    else if (!strcmp(argv[1], "out"))
      join(STDOUT_FILENO, sock1);
    else if (!strcmp(argv[1], "inout")) {
      if (fork() == 0)
        join(STDOUT_FILENO, sock1);
      else
        join(sock1, STDIN_FILENO);
    }
    else
      usage();
  }
  else usage();

  return 0;
}
