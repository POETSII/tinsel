// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

int main()
{
  // Create DebugLink
  DebugLinkParams debugLinkParams;
  debugLinkParams.numBoxesX = 1;
  debugLinkParams.numBoxesY = 1;
  debugLinkParams.useExtraSendSlot = false;
  debugLinkParams.max_connection_attempts=1;
  DebugLink* debugLink = new DebugLink(debugLinkParams);

  // Send char to thread 0
  debugLink->setDest(0, 0, 0, 0);
  debugLink->put(0, 0, 'X');
  printf("put done\n");

  // Receive char
  uint8_t c;
  uint32_t boardX, boardY, coreId, threadId;
  debugLink->get(&boardX, &boardY, &coreId, &threadId, &c);
  printf("Got byte '%c' from board (%d,%d) core %d thread %d\n",
    c, boardX, boardY, coreId, threadId);

  return 0;
}
