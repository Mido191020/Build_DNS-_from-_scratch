#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define QUERY_TIMEOUT_SEC 10

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <signal.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define CLOSESOCKET(s) closesocket(s)
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define INVALID_SOCKET -1
#endif

typedef struct pending_query {
    uint16_t              dns_id;
    unsigned char        *raw_query;
    size_t                raw_query_len;
    struct sockaddr_in    client_addr;
    time_t                sent_at;
    struct pending_query *next;
} pending_query_t;

static pending_query_t *pending_head = NULL;
static SOCKET local_sock = INVALID_SOCKET;
static SOCKET upstream_sock = INVALID_SOCKET;

static void die(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

static uint16_t read_dns_id(const char *packet) {
    return (uint16_t)(((uint8_t)packet[0] << 8) | (uint8_t)packet[1]);
}

static void pending_list_destroy(void) {
    pending_query_t *curr = pending_head;
    while (curr != NULL) {
        pending_query_t *next_node = curr->next;
        free(curr->raw_query);
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
    free(curr->raw_query);
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

static void handle_sigint(int sig) {
    (void)sig;
    printf("\nShutting down. Cleaning up memory...\n");
    pending_list_destroy();
    if (local_sock != INVALID_SOCKET) {
        CLOSESOCKET(local_sock);
        local_sock = INVALID_SOCKET;
    }
    if (upstream_sock != INVALID_SOCKET) {
        CLOSESOCKET(upstream_sock);
        upstream_sock = INVALID_SOCKET;
    }
#if defined(_WIN32)
    WSACleanup();
#endif
    exit(0);
}

int main(void) {
    signal(SIGINT, handle_sigint);

#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d) != 0) {
        return 1;
    }
#endif

    local_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (local_sock == INVALID_SOCKET) {
        die("failed to create local socket");
    }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(5353);
    if (inet_pton(AF_INET, "127.0.0.1", &local_addr.sin_addr) != 1) {
        die("invalid local bind address");
    }

    if (bind(local_sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        die("failed to bind local socket");
    }

    upstream_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (upstream_sock == INVALID_SOCKET) {
        die("failed to create upstream socket");
    }

    struct sockaddr_in google_addr;
    memset(&google_addr, 0, sizeof(google_addr));
    google_addr.sin_family = AF_INET;
    google_addr.sin_port = htons(53);
    if (inet_pton(AF_INET, "8.8.8.8", &google_addr.sin_addr) != 1) {
        die("invalid upstream address");
    }

    fd_set master_set;
    FD_ZERO(&master_set);
    FD_SET(local_sock, &master_set);
    FD_SET(upstream_sock, &master_set);

#if defined(_WIN32)
    const int select_nfds = 0;
#else
    const int select_nfds = MAX(local_sock, upstream_sock) + 1;
#endif

    printf("DNS Proxy (M2) is running on 127.0.0.1:5353...\n");

    while (1) {
        fd_set working_set = master_set;
        struct timeval timeout_window;
        timeout_window.tv_sec = 5;
        timeout_window.tv_usec = 0;

        const int activity = select(select_nfds, &working_set, NULL, NULL, &timeout_window);
        if (activity < 0) {
            perror("select error");
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
            const int bytes_read = recvfrom(local_sock, buffer, (int)sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);

            if (bytes_read >= 2) {
                const uint16_t query_id = read_dns_id(buffer);
                pending_query_t *new_node = (pending_query_t *)malloc(sizeof(pending_query_t));
                if (new_node != NULL) {
                    new_node->raw_query = (unsigned char *)malloc((size_t)bytes_read);
                    if (new_node->raw_query != NULL) {
                        memcpy(new_node->raw_query, buffer, (size_t)bytes_read);
                        new_node->raw_query_len = (size_t)bytes_read;
                        new_node->dns_id = query_id;
                        new_node->client_addr = client_addr;
                        new_node->sent_at = time(NULL);
                        new_node->next = pending_head;
                        pending_head = new_node;

                        if (sendto(upstream_sock, buffer, bytes_read, 0, (struct sockaddr *)&google_addr, (int)sizeof(google_addr)) < 0) {
                            remove_pending_node(NULL, new_node);
                            perror("failed to forward query upstream");
                        } else {
                            printf("[Client -> Proxy] Forwarded ID: %u\n", (unsigned int)query_id);
                        }
                    } else {
                        free(new_node);
                    }
                }
            }
        }

        if (FD_ISSET(upstream_sock, &working_set)) {
            char upstream_buffer[512];
            struct sockaddr_in upstream_addr;
            socklen_t upstream_len = (socklen_t)sizeof(upstream_addr);
            const int reply_len = recvfrom(upstream_sock, upstream_buffer, (int)sizeof(upstream_buffer), 0, (struct sockaddr *)&upstream_addr, &upstream_len);

            if (reply_len >= 2) {
                const uint16_t reply_id = read_dns_id(upstream_buffer);
                pending_query_t *prev = NULL;
                pending_query_t *curr = pending_head;

                while (curr != NULL) {
                    if (curr->dns_id == reply_id) {
                        if (sendto(local_sock, upstream_buffer, reply_len, 0, (struct sockaddr *)&curr->client_addr, (int)sizeof(curr->client_addr)) >= 0) {
                            printf("[Google -> Proxy -> Client] Handled ID: %u\n", (unsigned int)reply_id);
                        } else {
                            perror("failed to send response to client");
                        }
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
    }
    if (upstream_sock != INVALID_SOCKET) {
        CLOSESOCKET(upstream_sock);
    }
#if defined(_WIN32)
    WSACleanup();
#endif

    return 0;
}
