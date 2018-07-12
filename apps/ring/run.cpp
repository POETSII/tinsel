#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <HostLink.h>

int main()
{
  HostLink hostLink;

  // Download code
  hostLink.boot("code.v", "data.v");

  // Get start time
  printf("Starting\n");
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  // Trigger execution
  hostLink.go();

  // Wait for response
  uint32_t resp[4];
  hostLink.recv(resp);
  printf("Done\n");

  // Get finish time
  gettimeofday(&finish, NULL);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
