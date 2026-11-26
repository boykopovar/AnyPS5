#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "getwd check failed\n");
        std::abort();
    }
}

int main() {
    char expected[1024];
    Require(getcwd_nid_postfix(expected, sizeof(expected)) == expected);

    char buffer[1024];
    std::memset(buffer, 'x', sizeof(buffer));
    Require(getwd_nid_postfix(buffer) == buffer);
    Require(std::strcmp(buffer, expected) == 0);

    bool thrown = false;
    try {
        getwd_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
    return 0;
}
