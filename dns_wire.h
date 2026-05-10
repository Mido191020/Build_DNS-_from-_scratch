#ifndef DNS_WIRE_H
#define DNS_WIRE_H

#include <stddef.h>
#include <stdint.h>

char *build_dns_query(const char *hostname, size_t *out_len);

int is_dns_reply_valid(const unsigned char *buffer, size_t buffer_len, uint16_t sent_id);

char *pars_reply(
        const unsigned char *buffer,
        size_t buffer_len,
        uint16_t sent_id,
        char *out_buffer,
        size_t out_len
);

#endif