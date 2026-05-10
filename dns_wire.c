#include "dns_wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int encode_dns_name(const char *host, unsigned char *out) {
    const char *pos = host;
    const char *label = host;
    unsigned char *dst = out;

    while (*pos) {
        if (*pos == '.') {
            size_t l = (size_t)(pos - label);
            if (l == 0 || l > 63) return -1;

            *dst++ = (unsigned char)l;
            memcpy(dst, label, l);
            dst += l;

            label = pos + 1;
        }
        pos++;
    }

    size_t l = (size_t)(pos - label);
    if (l == 0 || l > 63) return -1;

    *dst++ = (unsigned char)l;
    memcpy(dst, label, l);
    dst += l;

    *dst++ = 0x00;

    return (int)(dst - out);
}

char *build_dns_query(const char *hostname, size_t *out_len) {
    unsigned char *buffer = malloc(512);
    if (!buffer) return NULL;

    unsigned char *p = buffer;

    uint16_t id = 0xAAAA;

    *p++ = (unsigned char)(id >> 8);
    *p++ = (unsigned char)(id & 0xFF);

    *p++ = 0x01;
    *p++ = 0x00;

    *p++ = 0x00; *p++ = 0x01;
    *p++ = 0x00; *p++ = 0x00;
    *p++ = 0x00; *p++ = 0x00;
    *p++ = 0x00; *p++ = 0x00;

    int name_len = encode_dns_name(hostname, p);
    if (name_len < 0) {
        free(buffer);
        return NULL;
    }

    p += name_len;

    *p++ = 0x00; *p++ = 0x01;
    *p++ = 0x00; *p++ = 0x01;

    *out_len = (size_t)(p - buffer);
    return (char *)buffer;
}

int is_dns_reply_valid(const unsigned char *buffer, size_t buffer_len, uint16_t sent_id) {
    if (buffer_len < 12) return 0;

    if (!(buffer[2] & 0x80)) return 0;

    uint16_t recv_id = (buffer[0] << 8) | buffer[1];
    if (recv_id != sent_id) return 0;

    if ((buffer[3] & 0x0F) != 0) return 0;

    uint16_t ancount = (buffer[6] << 8) | buffer[7];
    if (ancount == 0) return 0;

    return 1;
}

char *pars_reply(
        const unsigned char *buffer,
        size_t buffer_len,
        uint16_t sent_id,
        char *out_buffer,
        size_t out_len
) {
    if (!is_dns_reply_valid(buffer, buffer_len, sent_id)) {
        return NULL;
    }

    size_t offset = 12;

    while (offset < buffer_len && buffer[offset] != 0) {
        unsigned char len = buffer[offset];

        if ((len & 0xC0) == 0xC0) return NULL;

        if (offset + len + 1 >= buffer_len) return NULL;

        offset += len + 1;
    }

    if (offset + 5 >= buffer_len) return NULL;

    offset += 5;

    if (offset + 12 > buffer_len) return NULL;

    offset += 10;

    uint16_t rdlength = (buffer[offset] << 8) | buffer[offset + 1];

    if (rdlength != 4) return NULL;

    offset += 2;

    if (offset + 4 > buffer_len) return NULL;

    snprintf(out_buffer, out_len, "%u.%u.%u.%u",
             buffer[offset],
             buffer[offset + 1],
             buffer[offset + 2],
             buffer[offset + 3]);

    return out_buffer;
}
