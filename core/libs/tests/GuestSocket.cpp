#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "check failed (errno %d)\n", *__error_nid_postfix());
        std::exit(1);
    }
}

int main() {
    const int sock = socket_nid_postfix(2, 2, 0);
    Require(sock >= 0);

    int val = 1;
    Require(setsockopt_nid_postfix(sock, 0, 10, &val, -1) == -1);
    Require(*__error_nid_postfix() == 22);

    Require(setsockopt_nid_postfix(sock, 0, 10, &val, -100) == -1);
    Require(*__error_nid_postfix() == 22);

    Require(setsockopt_nid_postfix(sock, -1, 10, &val, sizeof(val)) == -1);
    Require(*__error_nid_postfix() == 22);

    Require(setsockopt_nid_postfix(sock, -99, 10, &val, sizeof(val)) == -1);
    Require(*__error_nid_postfix() == 22);

    Require(setsockopt_nid_postfix(sock, 9999, 10, &val, sizeof(val)) == -1);
    Require(*__error_nid_postfix() == 22);

    Require(setsockopt_nid_postfix(sock, 0, 12345, &val, sizeof(val)) == -1);
    Require(*__error_nid_postfix() == 42);

    Require(close_nid_postfix(sock) == 0);
    return 0;
}
