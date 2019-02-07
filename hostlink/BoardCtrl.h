#ifndef _BOARD_CTRL_H_
#define _BOARD_CTRL_H_

#include "DebugLinkFormat.h"

// Board control packet
// Payload contains a DebugLink packet
struct BoardCtrlPkt {
  uint8_t linkId;
  uint8_t payload[DEBUGLINK_MAX_PKT_BYTES];
};

#endif
