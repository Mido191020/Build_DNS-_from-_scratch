#ifndef DNS_PROXY_SERVER_H
#define DNS_PROXY_SERVER_H

int run_dns_proxy(const char *bind_ip,
                  unsigned short bind_port,
                  const char *upstream_ip,
                  unsigned short upstream_port);

#endif
