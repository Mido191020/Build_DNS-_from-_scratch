#ifndef DNS_QUERY_CLIENT_H
#define DNS_QUERY_CLIENT_H

int run_dns_query(const char *target_domain, const char *resolver_ip, unsigned short resolver_port);

#endif
