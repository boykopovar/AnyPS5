#include "prx/libkernel/include/Api.hpp"
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
    Require(usleep_nid_postfix(0) == 0, "usleep(0) returns 0");

    const auto before = std::chrono::steady_clock::now();
    const int result = usleep_nid_postfix(30000);
    const auto elapsed = std::chrono::steady_clock::now() - before;
    Require(result == 0, "usleep returns 0 after sleeping");
    Require(elapsed >= std::chrono::milliseconds(25), "usleep(30000) sleeps at least 25 ms");
    return 0;
}
