#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    *__error_nid_postfix() = 61;
    Require(usleep_nid_postfix(0) == 0, "usleep(0) returns 0");
    Require(*__error_nid_postfix() == 61, "usleep(0) preserves errno");
    Require(usleep_nid_postfix(1000) == 0, "usleep(1000) returns 0");
    Require(*__error_nid_postfix() == 61, "usleep(1000) preserves errno");

    const auto before = std::chrono::steady_clock::now();
    const int result = usleep_nid_postfix(30000);
    const auto elapsed = std::chrono::steady_clock::now() - before;
    Require(result == 0, "usleep returns 0 after sleeping");
    Require(*__error_nid_postfix() == 61, "usleep(30000) preserves errno");
    Require(elapsed >= std::chrono::milliseconds(25), "usleep(30000) sleeps at least 25 ms");
    return 0;
}
