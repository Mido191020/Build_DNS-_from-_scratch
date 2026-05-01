#include "dns_proxy_server.h"

int main(void) {
    return run_dns_proxy("127.0.0.1", 5353, "8.8.8.8", 53);
}
