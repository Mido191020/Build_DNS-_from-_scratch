#include "dns_proxy_server.h"
#include "net_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define QUERY_TIMEOUT_SEC 10

typedef struct pending_query {
    unsigned short dns_id;
    struct sockaddr_in client_addr;
    time_t sent_at;
    struct pending_query *next;
} pending_query_t;

static pending_query_t *pending_head = NULL;
static SOCKET local_sock = INVALID_SOCKET;
static SOCKET upstream_sock = INVALID_SOCKET;

static unsigned short read_dns_id(const char *packet) {
    return (unsigned short)(((unsigned char)packet[0] << 8) | (unsigned char)packet[1]);
}

static void pending_list_destroy(void) {
    pending_query_t *curr = pending_head;
    while (curr != NULL) {
        pending_query_t *next_node = curr->next;
        free(curr);
        curr = next_node;
    }
    pending_head = NULL;
}

static void remove_pending_node(pending_query_t *prev, pending_query_t *curr) {
    if (prev != NULL) {
        prev->next = curr->next;
    } else {
        pending_head = curr->next;
    }
    free(curr);
}

static void evict_timed_out_queries(void) {
    const time_t now = time(NULL);
    pending_query_t *prev = NULL;
    pending_query_t *curr = pending_head;

    while (curr != NULL) {
        pending_query_t *next_node = curr->next;
        if ((now - curr->sent_at) > QUERY_TIMEOUT_SEC) {
            printf("[Cleanup] Evicting stale ID: %u (Timed Out)\n", (unsigned int)curr->dns_id);
            remove_pending_node(prev, curr);
        } else {
            prev = curr;
        }
        curr = next_node;
    }
}

static int setup_socket_bind(SOCKET *sock, const char *bind_ip, unsigned short bind_port) {
    *sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (*sock == INVALID_SOCKET) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(bind_port);
    if (inet_pton(AF_INET, bind_ip, &addr.sin_addr) != 1) {
        CLOSESOCKET(*sock);
        *sock = INVALID_SOCKET;
        return -1;
    }
    if (bind(*sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        CLOSESOCKET(*sock);
        *sock = INVALID_SOCKET;
        return -1;
    }
    return 0;
}

int run_dns_proxy(const char *bind_ip,
                  unsigned short bind_port,
                  const char *upstream_ip,
                  unsigned short upstream_port) {
    if (bind_ip == NULL || upstream_ip == NULL) {
        return 1;
    }

    if (net_platform_init() != 0) {
        return 1;
    }

    if (setup_socket_bind(&local_sock, bind_ip, bind_port) != 0) {
        net_platform_cleanup();
        return 1;
    }

    upstream_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (upstream_sock == INVALID_SOCKET) {
        CLOSESOCKET(local_sock);
        local_sock = INVALID_SOCKET;
        net_platform_cleanup();
        return 1;
    }

    struct sockaddr_in upstream_addr;
    memset(&upstream_addr, 0, sizeof(upstream_addr));
    upstream_addr.sin_family = AF_INET;
    upstream_addr.sin_port = htons(upstream_port);
    if (inet_pton(AF_INET, upstream_ip, &upstream_addr.sin_addr) != 1) {
        CLOSESOCKET(local_sock);
        CLOSESOCKET(upstream_sock);
        local_sock = INVALID_SOCKET;
        upstream_sock = INVALID_SOCKET;
        net_platform_cleanup();
        return 1;
    }

    fd_set master_set;
    FD_ZERO(&master_set);
    FD_SET(local_sock, &master_set);
    FD_SET(upstream_sock, &master_set);

    printf("DNS proxy listening on %s:%u -> upstream %s:%u\n",
           bind_ip, (unsigned int)bind_port, upstream_ip, (unsigned int)upstream_port);

    while (1) {
        fd_set working_set = master_set;
        struct timeval timeout_window;
        timeout_window.tv_sec = 5;
        timeout_window.tv_usec = 0;

#if defined(_WIN32)
        const int activity = select(0, &working_set, NULL, NULL, &timeout_window);
#else
        const int activity = select(MAX(local_sock, upstream_sock) + 1,
                                    &working_set, NULL, NULL, &timeout_window);
#endif
        if (activity < 0) {
            break;
        }
        if (activity == 0) {
            evict_timed_out_queries();
            continue;
        }

        if (FD_ISSET(local_sock, &working_set)) {
            char buffer[512];
            struct sockaddr_in client_addr;
            socklen_t client_len = (socklen_t)sizeof(client_addr);
            const int bytes_read = recvfrom(local_sock, buffer, (int)sizeof(buffer), 0,
                                            (struct sockaddr *)&client_addr, &client_len);
            if (bytes_read >= 2) {
                const unsigned short query_id = read_dns_id(buffer);

                pending_query_t *new_node = (pending_query_t *)malloc(sizeof(pending_query_t));
                if (new_node != NULL) {
                    new_node->dns_id = query_id;
                    new_node->client_addr = client_addr;
                    new_node->sent_at = time(NULL);
                    new_node->next = pending_head;
                    pending_head = new_node;

                    if (sendto(upstream_sock, buffer, bytes_read, 0,
                               (struct sockaddr *)&upstream_addr, (int)sizeof(upstream_addr)) < 0) {
                        remove_pending_node(NULL, new_node);
                    }
                }
            }
        }

        if (FD_ISSET(upstream_sock, &working_set)) {
            char upstream_buffer[512];
            struct sockaddr_in source_addr;
            socklen_t source_len = (socklen_t)sizeof(source_addr);
            const int reply_len = recvfrom(upstream_sock, upstream_buffer, (int)sizeof(upstream_buffer), 0,
                                           (struct sockaddr *)&source_addr, &source_len);
            if (reply_len >= 2) {
                const unsigned short reply_id = read_dns_id(upstream_buffer);
                pending_query_t *prev = NULL;
                pending_query_t *curr = pending_head;
                while (curr != NULL) {
                    if (curr->dns_id == reply_id) {
                        sendto(local_sock, upstream_buffer, reply_len, 0,
                               (struct sockaddr *)&curr->client_addr, (int)sizeof(curr->client_addr));
                        remove_pending_node(prev, curr);
                        break;
                    }
                    prev = curr;
                    curr = curr->next;
                }
            }
        }

        evict_timed_out_queries();
    }

    pending_list_destroy();
    if (local_sock != INVALID_SOCKET) {
        CLOSESOCKET(local_sock);
        local_sock = INVALID_SOCKET;
    }
    if (upstream_sock != INVALID_SOCKET) {
        CLOSESOCKET(upstream_sock);
        upstream_sock = INVALID_SOCKET;
    }
    net_platform_cleanup();
    return 0;
}
