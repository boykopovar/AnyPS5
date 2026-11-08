#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <unistd.h>

#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"

namespace {

constexpr int Eperm = 1;
constexpr int Einval = 22;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u (error %d)\n", static_cast<unsigned>(location.line()), *__error_nid_postfix());
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

} // namespace

int main() {
    uid_t uid = getuid();
    uid_t euid = geteuid();
    gid_t gid = getgid();
    gid_t egid = getegid();

    Require(uid >= 0);
    Require(euid >= 0);
    Require(gid >= 0);
    Require(egid >= 0);

    Require(setuid(static_cast<uid_t>(-1)) == -1);
    Require(setgid(static_cast<gid_t>(-1)) == -1);

    return 0;
}
