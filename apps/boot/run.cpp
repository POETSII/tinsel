// SPDX-License-Identifier: BSD-2-Clause
#include <DebugLink.h>
#include <unistd.h>

void debugprintf(DebugLink* debugLink) {
  bool got = false;
  for (int rpt=0; rpt<5000; rpt++){
    usleep(100);
    while (debugLink->canGet()) {
      got = true;
      // Receive byte
      uint8_t byte;
      uint32_t x, y, c, t;
      debugLink->get(&x, &y, &c, &t, &byte);
      printf("[debugprintf] %d:%d:%d:%d: 0x%02X (%c)\n", x, y, c, t, byte, byte);
    }
  }
  // if (!got) printf("[debugprintf] <nothing>\n");
}

int main()
{
  // Create DebugLink
  printf("[apps/boot/run:main] started\n");
  DebugLinkParams debugLinkParams;
  debugLinkParams.numBoxesX = 1;
  debugLinkParams.numBoxesY = 1;
  debugLinkParams.useExtraSendSlot = 0;
  debugLinkParams.max_connection_attempts=5;
  DebugLink debugLink = DebugLink(debugLinkParams);
  printf("[apps/boot/run:main] debuglink connected, going to print loop\n");

  // Send char to thread 0
  debugLink.setDest(0, 0, 0, 0);
  debugLink.put(0, 0, 'X');
  printf("put done\n");

  while (1) {
    debugprintf(&debugLink);
  }

  return 0;
}
