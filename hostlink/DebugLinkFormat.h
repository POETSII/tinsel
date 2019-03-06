#ifndef _DEBUGLINK_FORMAT_H_
#define _DEBUGLINK_FORMAT_H_

// Full details of the packet format can be found in 'rtl/DebugLink.bsv'.

// DebugLink commands
#define DEBUGLINK_QUERY_IN  0
#define DEBUGLINK_QUERY_OUT 0
#define DEBUGLINK_SET_DEST  1
#define DEBUGLINK_STD_IN    2
#define DEBUGLINK_STD_OUT   2
#define DEBUGLINK_READY     255

// Maximum size of a DebugLink packet
#define DEBUGLINK_MAX_PKT_BYTES 4

// Size of a packet to DebugLink
inline int toDebugLinkSize(uint8_t cmd)
{
  switch (cmd) {
    case DEBUGLINK_QUERY_IN: return 2;
    case DEBUGLINK_SET_DEST: return 3;
    case DEBUGLINK_STD_IN: return 2;
    default:
      fprintf(stderr, "toDebugLinkSize: unexpected command %d\n", cmd);
      exit(EXIT_FAILURE);
  }
}

// Size of a packet from DebugLink
inline int fromDebugLinkSize(uint8_t cmd)
{
  switch (cmd) {
    case DEBUGLINK_QUERY_OUT: return 2;
    case DEBUGLINK_STD_OUT: return 4;
    case DEBUGLINK_READY: return 1;
    default:
      fprintf(stderr, "fromDebugLinkSize: unexpected command %d\n", cmd);
      exit(EXIT_FAILURE);
  }
}

#endif
