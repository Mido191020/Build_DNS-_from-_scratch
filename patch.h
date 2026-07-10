#ifndef PATCH_H
#define PATCH_H
#include <stdint.h>

#define htons(x) ((x) == 5353 ? htons(5354) : htons(x))

uintptr_t my_socket_wrapper(int domain, int type, int protocol);
#define socket my_socket_wrapper

#endif
