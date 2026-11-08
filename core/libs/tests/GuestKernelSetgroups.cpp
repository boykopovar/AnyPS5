#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <unistd.h>

#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"

namespace {

constexpr int Efault = 14;
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
    gid_t groups[1];
    groups[0] = 0;

    Require(setgroups(1000000, groups) == -1 && Error() == Einval);

    Require(setgroups(1, nullptr) == -1 && Error() == Efault);
    Require(setgroups(1, reinterpret_cast<gid_t*>(0x10)) == -1 && Error() == Efault);

    return 0;
}
