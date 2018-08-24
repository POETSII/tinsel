#ifndef _POWER_LINK_H_
#define _POWER_LINK_H_

// Functions to communicate with Cambridge's Power Board
// (Very basic functionality for now)

// Enable or disable power to all worker FPGAs
void powerEnable(int enable);

// Wait for FPGAs to be detected after powerup
void waitForFPGAs(int numFPGAs);

#endif
