#include "dns_wire.h"

#include <stdio.h>
#include <stdlib.h>

int encode_dns_name(const char *domain, unsigned char *out_buffer) {
    if (domain == NULL || out_buffer == NULL) {
        return -1;
    }

    const char *in_ptr = domain;
    unsigned char *out_ptr = out_buffer;
    unsigned char *start = out_buffer;

    unsigned char label_len = 0;
    unsigned char *len_ptr = out_ptr++;

    while (*in_ptr != '\0') {
        if (*in_ptr == '.') {
            if (label_len == 0 || label_len > 63) {
                return -1;
            }
            *len_ptr = label_len;
            label_len = 0;
            len_ptr = out_ptr++;
        } else {
            if (label_len >= 63) {
                return -1;
            }
            *out_ptr++ = (unsigned char)*in_ptr;
            label_len++;
        }
        in_ptr++;
    }

    if (label_len == 0 || label_len > 63) {
        return -1;
    }

    *len_ptr = label_len;
    *out_ptr++ = 0x00;
    return (int)(out_ptr - start);
}

char *build_dns_query(const char *hostname, size_t *out_len) {
    if (hostname == NULL || out_len == NULL) {
        return NULL;
    }

    char *buffer = (char *)malloc(512);
    if (buffer == NULL) {
        return NULL;
    }

    char *write_ptr = buffer;
    *write_ptr++ = (char)0xAA;
    *write_ptr++ = (char)0xAA;
    *write_ptr++ = 0x01;
    *write_ptr++ = 0x00;
    *write_ptr++ = 0x00;
    *write_ptr++ = 0x01;
    *write_ptr++ = 0x00;
    *write_ptr++ = 0x00;
    *write_ptr++ = 0x00;
    *write_ptr++ = 0x00;
    *write_ptr++ = 0x00;
    *write_ptr++ = 0x00;

    const int name_len = encode_dns_name(hostname, (unsigned char *)write_ptr);
    if (name_len < 0) {
        free(buffer);
        return NULL;
    }

    write_ptr += name_len;
    *write_ptr++ = 0x00;
    *write_ptr++ = 0x01;
    *write_ptr++ = 0x00;
    *write_ptr++ = 0x01;

    *out_len = (size_t)(write_ptr - buffer);
    return buffer;
}

int parse_dns_reply(const char *buf, size_t len, char *out_ip, size_t ip_len) {
    if (buf == NULL || out_ip == NULL || ip_len < 16 || len < 12) {
        return -1;
    }

    size_t offset = 12;
    while (offset < len) {
        const unsigned char byte = (unsigned char)buf[offset];
        if ((byte & 0xC0) == 0xC0) {
            if (offset + 1 >= len) {
                return -1;
            }
            offset += 2;
            break;
        }
        if (byte == 0) {
            offset += 1;
            break;
        }
        offset += (size_t)byte + 1;
        if (offset > len) {
            return -1;
        }
    }

    if (offset + 4 > len) {
        return -1;
    }
    offset += 4;

    if (offset + 2 > len) {
        return -1;
    }
    offset += 2;

    const unsigned short type =
            (unsigned short)(((unsigned char)buf[offset] << 8) |
                             (unsigned char)buf[offset + 1]);
    offset += 2;

    if (offset + 2 + 4 + 2 > len) {
        return -1;
    }
    offset += 2;
    offset += 4;

    const unsigned short rdlength =
            (unsigned short)(((unsigned char)buf[offset] << 8) |
                             (unsigned char)buf[offset + 1]);
    offset += 2;

    if (offset + rdlength > len) {
        return -1;
    }

    if (type == 1 && rdlength == 4) {
        const unsigned char ip1 = (unsigned char)buf[offset];
        const unsigned char ip2 = (unsigned char)buf[offset + 1];
        const unsigned char ip3 = (unsigned char)buf[offset + 2];
        const unsigned char ip4 = (unsigned char)buf[offset + 3];
        snprintf(out_ip, ip_len, "%u.%u.%u.%u", ip1, ip2, ip3, ip4);
        return 0;
    }

    return -1;
}
