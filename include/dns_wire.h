#ifndef DNS_WIRE_H
#define DNS_WIRE_H

#include <stddef.h>

int encode_dns_name(const char *domain, unsigned char *out_buffer);
char *build_dns_query(const char *hostname, size_t *out_len);
int parse_dns_reply(const char *buf, size_t len, char *out_ip, size_t ip_len);

#endif
