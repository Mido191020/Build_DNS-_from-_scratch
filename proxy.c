#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#define closesocket close
#endif

typedef struct Node {
    uint16_t ID;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    char *request_Buffer;
    size_t request_size;
    struct Node *next;
} request_node;

request_node *Head = NULL;

int add(uint16_t id, struct sockaddr_in *client_data, socklen_t addr_len, char *buffer, size_t request_size) {
    if (client_data == NULL || buffer == NULL || request_size == 0) {
        fprintf(stderr, "empty data\n");
        return -1;
    }

    request_node *new_node = malloc(sizeof(request_node));
    if (new_node == NULL) return -1;

    new_node->ID = id;
    new_node->client_addr = *client_data;
    new_node->addr_len = addr_len;
    new_node->request_Buffer = malloc(request_size);
    if (new_node->request_Buffer == NULL) {
        free(new_node);
        return -1;
    }
    memcpy(new_node->request_Buffer, buffer, request_size);
    new_node->request_size = request_size;
    new_node->next = Head;
    Head = new_node;
    return 0;
}

request_node *find(uint16_t ID) {
    request_node *temp = Head;
    while (temp != NULL) {
        if (temp->ID == ID) return temp;
        temp = temp->next;
    }
    return NULL;
}

void delete(uint16_t ID) {
    if (Head == NULL) return;

    request_node *temp = Head;
    request_node *prev = NULL;

    if (temp->ID == ID) {
        Head = temp->next;
        free(temp->request_Buffer);
        free(temp);
        return;
    }

    while (temp != NULL && temp->ID != ID) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) return;

    prev->next = temp->next;
    free(temp->request_Buffer);
    free(temp);
}

int main(void) {
#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d) != 0) {
        perror("WSAStartup");
        return 1;
    }
#endif
    int local_socket;
    local_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (local_socket < 0) {
        perror("failed to create local socket");
        return 1;
    }

    struct sockaddr_in local_address;
    memset(&local_address, 0, sizeof(local_address));
    local_address.sin_family = AF_INET;
    local_address.sin_port = htons(5353);
    char *ip_string = "127.0.0.1";
    if (!inet_pton(AF_INET, ip_string, &(local_address.sin_addr))) {
        perror("invalid IP");
        closesocket(local_socket);
        return 1;
    }

    if (bind(local_socket, (struct sockaddr *)&local_address, sizeof(local_address)) < 0) {
        perror("bind fail");
        closesocket(local_socket);
        return 1;
    }

    int upstream_sock;
    upstream_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (upstream_sock < 0) {
        perror("failed to create upstream socket");
        closesocket(local_socket);
        return 1;
    }

    struct sockaddr_in upstream_server_address;
    memset(&upstream_server_address, 0, sizeof(upstream_server_address));
    upstream_server_address.sin_family = AF_INET;
    upstream_server_address.sin_port = htons(53);
    char *google_IP = "8.8.8.8";
    if (!inet_pton(AF_INET, google_IP, &upstream_server_address.sin_addr)) {
        perror("Invalid IP");
        closesocket(local_socket);
        closesocket(upstream_sock);
        return 1;
    }

#if !defined(_WIN32)
    // Configure sockets to non-blocking mode on Linux/WSL
    int flags = fcntl(local_socket, F_GETFL, 0);
    if (flags < 0) { perror("fcntl F_GETFL local_socket"); return 1; }
    if (fcntl(local_socket, F_SETFL, flags | O_NONBLOCK) < 0) { perror("fcntl F_SETFL local_socket"); return 1; }

    flags = fcntl(upstream_sock, F_GETFL, 0);
    if (flags < 0) { perror("fcntl F_GETFL upstream_sock"); return 1; }
    if (fcntl(upstream_sock, F_SETFL, flags | O_NONBLOCK) < 0) { perror("fcntl F_SETFL upstream_sock"); return 1; }

    // Set up epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        closesocket(local_socket);
        closesocket(upstream_sock);
        return 1;
    }

    struct epoll_event ev, events[10];

    ev.events = EPOLLIN;
    ev.data.fd = local_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, local_socket, &ev) < 0) {
        perror("epoll_ctl local_socket");
        close(epoll_fd);
        closesocket(local_socket);
        closesocket(upstream_sock);
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = upstream_sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, upstream_sock, &ev) < 0) {
        perror("epoll_ctl upstream_sock");
        close(epoll_fd);
        closesocket(local_socket);
        closesocket(upstream_sock);
        return 1;
    }
#endif

    while (1) {
#if !defined(_WIN32)
        int nfds = epoll_wait(epoll_fd, events, 10, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == local_socket) {
                while (1) {
                    unsigned char buffer[1024];
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);

                    int received_bytes = recvfrom(local_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);

                    if (received_bytes < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        perror("recvfrom client failed");
                        break;
                    }

                    if (received_bytes < 2) {
                        perror("Packet too short to contain DNS ID");
                        continue;
                    }

                    uint16_t id = (buffer[0] << 8) | buffer[1];
                    add(id, &client_addr, client_len, (char *)buffer, received_bytes);

                    sendto(upstream_sock, buffer, received_bytes, 0, (struct sockaddr *)&upstream_server_address, sizeof(upstream_server_address));
                }
            }
            else if (fd == upstream_sock) {
                while (1) {
                    unsigned char response_buffer[1024];
                    struct sockaddr_in google_addr;
                    socklen_t google_len = sizeof(google_addr);

                    int received_bytes = recvfrom(upstream_sock, response_buffer, sizeof(response_buffer), 0, (struct sockaddr *)&google_addr, &google_len);

                    if (received_bytes < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        perror("recvfrom upstream failed");
                        break;
                    }

                    if (google_addr.sin_addr.s_addr != upstream_server_address.sin_addr.s_addr || 
                        google_addr.sin_port != upstream_server_address.sin_port) {
                        perror("Dropped packet from untrusted source");
                        continue;
                    }

                    if (received_bytes < 2) {
                        perror("Upstream packet too short to contain DNS ID");
                        continue;
                    }

                    uint16_t id = (response_buffer[0] << 8) | response_buffer[1];

                    request_node *node = find(id);
                    if (!node) {
                        perror("no matching client found");
                        continue;
                    }

                    sendto(local_socket, response_buffer, received_bytes, 0, (struct sockaddr *)&node->client_addr, node->addr_len);
                    delete(id);
                }
            }
        }
#else
        // Sleep fallback to prevent high CPU utilization on Windows if compiled natively
        Sleep(1000);
#endif
    }

    closesocket(local_socket);
    closesocket(upstream_sock);
#if !defined(_WIN32)
    close(epoll_fd);
#endif
    return 0;
}