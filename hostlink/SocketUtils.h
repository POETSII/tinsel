// SPDX-License-Identifier: BSD-2-Clause
#ifndef _SOCKET_UTILS_H_
#define _SOCKET_UTILS_H_

// Check if connection is alive
bool socketAlive(int conn);

// Can receive from connection?
bool socketCanGet(int conn);

// Can send on connection?
bool socketCanPut(int conn);

// Read exactly numBytes from socket, if any data is available
// Return < 0 on error, 0 if no data available, and 1 on success
int socketGet(int fd, char* buf, int numBytes);

// Either send exactly numBytes to a socket, or don't send anything
// Return < 0 on error, 0 if no data can be sent, and 1 on success
int socketPut(int fd, char* buf, int numBytes);

// Either send exactly numBytes to a socket, or don't send anything
// Return < 0 on error, 0 if no data can be sent, and 1 on success
int socketPut(int fd, char* buf, int numBytes);

// Read exactly numBytes from socket, blocking
void socketBlockingGet(int fd, char* buf, int numBytes);

// Read up to numBytes from socket. Returns if it would block.
void socketNonBlockingGet(int fd, char* buf, int numBytes, int *pGot);

// Either send exactly numBytes to a socket, blocking
void socketBlockingPut(int fd, char* buf, int numBytes);

// Create TCP connection to given host/port
/*!
    \param returnIfCantConnect In the case that we cant connect, return -1 rather than exit
*/
int socketConnectTCP(const char* hostname, int port, bool returnIfCantConnect=false);


#endif
