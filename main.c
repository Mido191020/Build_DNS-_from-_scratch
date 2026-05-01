#include <stdio.h>
#include <stdlib.h>
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

    const char *target_domain = "example.com";

    // ===== BUILD QUERY =====
    size_t query_len = 0;
    char *query_buf = build_dns_query(target_domain, &query_len);
    if (!query_buf) return 1;

    // ===== SOCKET =====
    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        free(query_buf);
        return 1;
    }

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &dest.sin_addr);

    // ===== SEND =====
    int bytes_sent = sendto(sockfd, query_buf, query_len, 0,
                            (struct sockaddr *)&dest, sizeof(dest));

    if (bytes_sent < 0) {
        free(query_buf);
        CLOSESOCKET(sockfd);
        return 1;
    }

    // IMPORTANT: free immediately after send (per mentor rule)
    free(query_buf);
    query_buf = NULL;

    // ===== TIMEOUT (select) =====
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    int sel = select(sockfd + 1, &rfds, NULL, NULL, &tv);

    if (sel == 0) {
        printf("Timeout waiting for DNS response\n");
        CLOSESOCKET(sockfd);
        return 1;
    }

    if (sel < 0) {
        CLOSESOCKET(sockfd);
        return 1;
    }

    // ===== RECEIVE =====
    char reply_buffer[1024];
    memset(reply_buffer, 0, sizeof(reply_buffer));

    struct sockaddr_in src_addr;
    memset(&src_addr, 0, sizeof(src_addr));
    socklen_t src_len = sizeof(src_addr);

    int reply_len = recvfrom(sockfd, reply_buffer, sizeof(reply_buffer), 0,
                             (struct sockaddr *)&src_addr, &src_len);

    if (reply_len < 0) {
        CLOSESOCKET(sockfd);
        return 1;
    }

    // ===== PARSE =====
    char resolved_ip[16];
    if (parse_dns_reply(reply_buffer, reply_len,
                        resolved_ip, sizeof(resolved_ip)) == 0) {
        printf(">>> Resolved IP: %s <<<\n", resolved_ip);
    } else {
        printf("Failed to parse A record\n");
    }

    // ===== CLEANUP =====
    CLOSESOCKET(sockfd);

#if defined(_WIN32)
    WSACleanup();
#endif

    return 0;
}
