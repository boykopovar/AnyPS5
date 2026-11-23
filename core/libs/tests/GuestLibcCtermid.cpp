#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ctermid check failed at line %u\n", location.line());
        std::abort();
    }
}

}

int main() {
    char* shared = ctermid_nid_postfix(nullptr);
    Require(shared != nullptr);
    Require(std::strcmp(shared, "/dev/tty") == 0);

    char buffer[1024];
    std::memset(buffer, 'x', sizeof(buffer));
    char* returned = ctermid_nid_postfix(buffer);
    Require(returned == buffer);
    Require(std::strcmp(buffer, "/dev/tty") == 0);
    Require(buffer[8] == '\0' && buffer[9] == 'x');
    return 0;
}
