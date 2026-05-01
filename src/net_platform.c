#include "net_platform.h"

int net_platform_init(void) {
#if defined(_WIN32)
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return -1;
    }
#endif
    return 0;
}

void net_platform_cleanup(void) {
#if defined(_WIN32)
    WSACleanup();
#endif
}
