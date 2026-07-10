#include <winsock2.h>
#include <stdio.h>
#include <stdint.h>

// Undefine the socket macro so this file can call the real winsock socket function
#undef socket

__attribute__((constructor)) void init_winsock() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    printf("init_winsock: constructor executing\n");
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
    } else {
        printf("init_winsock: WSAStartup succeeded\n");
    }
}

__attribute__((destructor)) void cleanup_winsock() {
    WSACleanup();
    printf("cleanup_winsock: destructor executing\n");
}

uintptr_t my_socket_wrapper(int domain, int type, int protocol) {
    SOCKET s = socket(domain, type, protocol);
    printf("my_socket_wrapper: socket(%d, %d, %d) returned %I64u (0x%I64X)\n", 
           domain, type, protocol, (unsigned long long)s, (unsigned long long)s);
    if (s == INVALID_SOCKET) {
        fprintf(stderr, "my_socket_wrapper: WSAGetLastError() = %d\n", WSAGetLastError());
    }
    return (uintptr_t)s;
}
