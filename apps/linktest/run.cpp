// Simple non-pipelined connectivity test using boot loader
// Check that every core can send a message to every other core

#include <stdio.h>
#include <stdlib.h>
#include <HostLink.h>
#include <boot.h>

int main()
{
  HostLink hostLink;

  // Create ping command
  BootReq req;
  req.cmd = PingCmd;
  req.args[0] = 1;

  // Send a ping between every pair of cores
  uint32_t count = 0;
  for (int x1 = 0; x1 < TinselMeshXLenWithinBox; x1++)
    for (int y1 = 0; y1 < TinselMeshYLenWithinBox; y1++)
      for (int i1 = 0; i1 < (1 << TinselLogCoresPerBoard); i1++)
        for (int x2 = 0; x2 < TinselMeshXLenWithinBox; x2++)
          for (int y2 = 0; y2 < TinselMeshYLenWithinBox; y2++)
            for (int i2 = 0; i2 < (1 << TinselLogCoresPerBoard); i2++) {
              uint32_t dest = hostLink.toAddr(x1, y1, i1, 0);
              uint32_t resp[4];
              req.args[1] = hostLink.toAddr(x2, y2, i2, 0);
              hostLink.send(dest, 1, &req);
              hostLink.recv(resp);
              if (resp[0] == req.args[1]) {
                printf(".");
                //fflush(stdout);
              }
              else {
                printf("\nInvalid result from ping %d -> %d\n",
                         dest, req.args[1]);
                return -1;
              }
              count++;
              if ((count%64) == 0) printf("\n");
            }

  printf("\nPassed\n");

  return 0;
}
