#include "dns_query_client.h"
#include "dns_wire.h"
#include "net_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int run_dns_query(const char *target_domain, const char *resolver_ip, unsigned short resolver_port) {
    if (target_domain == NULL || resolver_ip == NULL) {
        return 1;
    }

    if (net_platform_init() != 0) {
        return 1;
    }

    size_t query_len = 0;
    char *query_buf = build_dns_query(target_domain, &query_len);
    if (query_buf == NULL) {
        net_platform_cleanup();
        return 1;
    }

    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        free(query_buf);
        net_platform_cleanup();
        return 1;
    }

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(resolver_port);
    if (inet_pton(AF_INET, resolver_ip, &dest.sin_addr) != 1) {
        free(query_buf);
        CLOSESOCKET(sockfd);
        net_platform_cleanup();
        return 1;
    }

    const int bytes_sent =
            sendto(sockfd, query_buf, (int)query_len, 0, (struct sockaddr *)&dest, sizeof(dest));
    free(query_buf);
    if (bytes_sent < 0) {
        CLOSESOCKET(sockfd);
        net_platform_cleanup();
        return 1;
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;

#if defined(_WIN32)
    const int sel = select(0, &rfds, NULL, NULL, &tv);
#else
    const int sel = select(sockfd + 1, &rfds, NULL, NULL, &tv);
#endif
    if (sel <= 0) {
        CLOSESOCKET(sockfd);
        net_platform_cleanup();
        return 1;
    }

    char reply_buffer[1024];
    memset(reply_buffer, 0, sizeof(reply_buffer));
    struct sockaddr_in src_addr;
    memset(&src_addr, 0, sizeof(src_addr));
    socklen_t src_len = (socklen_t)sizeof(src_addr);

    const int reply_len =
            recvfrom(sockfd, reply_buffer, sizeof(reply_buffer), 0,
                     (struct sockaddr *)&src_addr, &src_len);
    if (reply_len < 0) {
        CLOSESOCKET(sockfd);
        net_platform_cleanup();
        return 1;
    }

    char resolved_ip[16];
    if (parse_dns_reply(reply_buffer, (size_t)reply_len, resolved_ip, sizeof(resolved_ip)) == 0) {
        printf("Resolved IP: %s\n", resolved_ip);
    } else {
        printf("Failed to parse A record\n");
    }

    CLOSESOCKET(sockfd);
    net_platform_cleanup();
    return 0;
}
