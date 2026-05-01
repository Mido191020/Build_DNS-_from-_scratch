#ifndef NET_PLATFORM_H
#define NET_PLATFORM_H

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define CLOSESOCKET(s) closesocket(s)
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define INVALID_SOCKET -1
#endif

int net_platform_init(void);
void net_platform_cleanup(void);

#endif
