#ifndef _POWER_LINK_H_
#define _POWER_LINK_H_

// Functions to communicate with Cambridge Power Monitor
// (Only basic functionality for now)

// Open a power link and return file descriptor, given device file
int powerInit(char* dev);

// Send a command over a power link
void powerPutCmd(int fd, char* cmd, char* resp, int respSize);

// Enable or disable power to all worker FPGAs
void powerEnable(int enable);

// Disable then enable power to all worker FPGAs
void powerReset();

#endif
