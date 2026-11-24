#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "sys_siglist check failed at line %u\n", location.line());
        std::abort();
    }
}

}

int main() {
    Require(sys_nsig_nid_postfix == 32);
    Require(std::strcmp(sys_siglist_nid_postfix[0], "Signal 0") == 0);
    Require(std::strcmp(sys_siglist_nid_postfix[2], "Interrupt") == 0);
    Require(std::strcmp(sys_siglist_nid_postfix[15], "Terminated") == 0);
    Require(std::strcmp(sys_siglist_nid_postfix[30], "User defined signal 1") == 0);
    Require(std::strcmp(sys_siglist_nid_postfix[31], "User defined signal 2") == 0);
    return 0;
}
