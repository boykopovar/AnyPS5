#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "confstr check failed\n");
        std::abort();
    }
}

int main() {
    constexpr int CsPath = 1;
    constexpr int CsWidth = 14;
    constexpr const char Path[] = "/usr/bin:/bin:/usr/sbin:/sbin";

    Require(confstr_nid_postfix(CsPath, nullptr, 0) == sizeof(Path));

    char buffer[64];
    std::memset(buffer, 'x', sizeof(buffer));
    Require(confstr_nid_postfix(CsPath, buffer, sizeof(buffer)) == sizeof(Path));
    Require(std::strcmp(buffer, Path) == 0);

    std::memset(buffer, 'x', sizeof(buffer));
    Require(confstr_nid_postfix(CsPath, buffer, 5) == sizeof(Path));
    Require(std::strcmp(buffer, "/usr") == 0);

    Require(confstr_nid_postfix(CsWidth, buffer, sizeof(buffer)) == sizeof("_POSIX_V6_LP64_OFF64"));
    Require(std::strcmp(buffer, "_POSIX_V6_LP64_OFF64") == 0);

    Require(confstr_nid_postfix(8, buffer, sizeof(buffer)) == 1);
    Require(buffer[0] == '\0');

    Require(confstr_nid_postfix(2, buffer, sizeof(buffer)) == sizeof("unsupported programming environment"));
    Require(std::strcmp(buffer, "unsupported programming environment") == 0);

    *__error_nid_postfix() = 0;
    Require(confstr_nid_postfix(999, buffer, sizeof(buffer)) == 0);
    Require(*__error_nid_postfix() == 22);
    return 0;
}
