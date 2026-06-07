#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define CLOSESOCKET(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define INVALID_SOCKET -1
#endif

#include "dns_wire.h"

int main(void) {

#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2,2), &d)) {
        return 1;
    }
#endif

    const char *domain = "example.com";

    size_t query_len;
    char *dns_query = build_dns_query(domain, &query_len);
    if (!dns_query) {
        perror("build_dns_query");
        return 1;
    }

    uint16_t sent_id = (uint16_t)(((unsigned char)dns_query[0] << 8) |
                                  (unsigned char)dns_query[1]);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        perror("socket");
        free(dns_query);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);

    if (inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr) <= 0) {
        perror("inet_pton");
        free(dns_query);
        CLOSESOCKET(sock);
        return 1;
    }

    if (query_len > INT_MAX) {
        fprintf(stderr, "DNS query too large\n");
        free(dns_query);
        CLOSESOCKET(sock);
        return 1;
    }

    if (sendto(sock, dns_query, (int)query_len, 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("sendto");
        free(dns_query);
        CLOSESOCKET(sock);
        return 1;
    }

    free(dns_query);

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;

#if defined(_WIN32)
    int sel = select(0, &rfds, NULL, NULL, &tv);
#else
    int sel = select((int)sock + 1, &rfds, NULL, NULL, &tv);
#endif

    if (sel == 0) {
        printf("Timeout\n");
        CLOSESOCKET(sock);
        return 1;
    }

    if (sel < 0) {
        perror("select");
        CLOSESOCKET(sock);
        return 1;
    }

    char buffer[1024];
    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);

    int n = recvfrom(sock, buffer, (int)sizeof(buffer), 0,
                      (struct sockaddr *)&src, &srclen);

    if (n < 0) {
        perror("recvfrom");
        CLOSESOCKET(sock);
        return 1;
    }

    char ip[16];

    if (!pars_reply((const unsigned char *)buffer, (size_t)n, sent_id, ip, sizeof(ip))) {
        printf("Failed to parse DNS response\n");
        CLOSESOCKET(sock);
        return 1;
    }

    printf("Resolved IP: %s\n", ip);

    CLOSESOCKET(sock);
    return 0;
}
