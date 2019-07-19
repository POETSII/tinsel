#ifndef _LINKRATE_H_
#define _LINKRATE_H_

// Total number of boxes available
#define BOXES_X 1
#define BOXES_Y 1

// Assumed clock frequency
#define MHZ 250

// Each thread one source FPGA sends N messages
// to same thread on a destination FPGA.
#define NumMsgs 100000

#endif
