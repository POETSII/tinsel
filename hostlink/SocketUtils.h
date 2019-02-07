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

// Create TCP connection to given host/port
int socketConnectTCP(const char* hostname, int port);

#endif
