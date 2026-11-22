#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "kernel sys development mode: %s\n", message);
        std::abort();
    }
}

} // namespace

int main() {
    *__error_nid_postfix() = 123;
    Require(__sys_is_development_mode_nid_postfix() == 0, "the first call did not report development mode disabled");
    Require(*__error_nid_postfix() == 123, "a successful mode query changed guest errno");
    Require(__sys_is_development_mode_nid_postfix() == 0, "a second call changed the answer");
    std::puts("guest_kernel_sys_development_mode passed");
    return 0;
}
