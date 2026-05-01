#include "dns_query_client.h"

int main(void) {
    return run_dns_query("example.com", "8.8.8.8", 53);
}
