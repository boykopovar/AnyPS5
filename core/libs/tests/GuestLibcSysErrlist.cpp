#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "sys_errlist check failed at line %u\n", location.line());
        std::abort();
    }
}

}

int main() {
    Require(sys_nerr_nid_postfix == 97);
    Require(std::strcmp(sys_errlist_nid_postfix[0], "No error: 0") == 0);
    Require(std::strcmp(sys_errlist_nid_postfix[2], "No such file or directory") == 0);
    Require(std::strcmp(sys_errlist_nid_postfix[96], "Previous owner died") == 0);
    Require(std::strcmp(sys_errlist_nid_postfix[95], "State not recoverable") == 0);
    Require(std::strcmp(sys_errlist_nid_postfix[30], "Read-only filesystem") == 0);
    Require(std::strcmp(strerror_nid_postfix(2), sys_errlist_nid_postfix[2]) == 0);
    Require(std::strcmp(strerror_nid_postfix(94), sys_errlist_nid_postfix[94]) == 0);
    return 0;
}
