#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>

int main() {
    using Add = int (APS5_VABI *)(int, int);
    void* extension = dlopen_nid_postfix("guest_module_fixture.prx", 2);
    Require(extension != nullptr);
    auto add = reinterpret_cast<Add>(dlsym_nid_postfix(extension, "GuestModuleAdd"));
    Require(add && add(19, 23) == 42);
    Require(dlclose_nid_postfix(extension) == 0);
    Require(dlopen_nid_postfix("guest_module_fixture.other.prx", 2) == nullptr);
    return 0;
}
