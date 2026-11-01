#include "prx/libkernel_sys/include/AdditionalExports.hpp"

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
    Require(__sys_is_development_mode_nid_postfix() == 0, "the first call did not answer 0 for a retail console");
    Require(__sys_is_development_mode_nid_postfix() == 0, "a second call changed the answer");
    std::puts("guest_kernel_sys_development_mode passed");
    return 0;
}
