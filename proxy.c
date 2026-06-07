#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ws2tcpip.h>

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
    if (client_data == NULL || buffer == NULL || buffer[0] == '\0') {
        perror("empty data");
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

    while (1) {
        unsigned char buffer[1024];
        struct sockaddr_in client_addr;
        socklen_t client_length = sizeof(client_addr);

        int receved_bytes = recvfrom(local_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_length);
        if (receved_bytes < 0) {
            perror("failed to receive bytes from client");
            continue;
        }
        if (receved_bytes < 2) {
            perror("Packet too short to contain DNS ID");
            continue;
        }

        uint16_t recv_id = (buffer[0] << 8) | buffer[1];

        int add_client = add(recv_id, &client_addr, client_length, buffer, receved_bytes);
        if (add_client < 0) {
            perror("failed to add client to queue");
            continue;
        }

        if (sendto(upstream_sock, buffer, receved_bytes, 0, (struct sockaddr *)&upstream_server_address, sizeof(upstream_server_address)) < 0) {
            perror("failed to send packet to upstream");
            delete(recv_id);
            continue;
        }

        unsigned char response_buffer[1024];
        struct sockaddr_in google_addr;
        socklen_t google_addr_len = sizeof(google_addr);

        int receved_goole_reponse = recvfrom(upstream_sock, response_buffer, sizeof(response_buffer), 0, (struct sockaddr *)&google_addr, &google_addr_len);
        if (receved_goole_reponse < 0) {
            perror("failed to receive google response");
            continue;
        }

        if (google_addr.sin_addr.s_addr != upstream_server_address.sin_addr.s_addr ||
            google_addr.sin_port != upstream_server_address.sin_port) {
            perror("Dropped packet from untrusted source");
            continue;
        }

        if (receved_goole_reponse < 2) {
            perror("Upstream packet too short to contain DNS ID");
            continue;
        }

        uint16_t google_response_ID = (response_buffer[0] << 8) | response_buffer[1];

        request_node *temp1 = find(google_response_ID);
        if (!temp1) {
            perror("invalid or unmatched ID received from upstream");
            continue;
        }

        if (sendto(local_socket, response_buffer, receved_goole_reponse, 0, (struct sockaddr *)&temp1->client_addr, temp1->addr_len) < 0) {
            perror("failed to send packet back to client");
        }

        delete(google_response_ID);
    }

    closesocket(local_socket);
    closesocket(upstream_sock);
    return 0;
}