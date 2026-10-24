#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
extern "C" {
void* APS5_VABI dlopen_nid_postfix(const char*, int);
void* APS5_VABI dlsym_nid_postfix(void*, const char*);
int APS5_VABI dlclose_nid_postfix(void*);
}
static void Require(bool value, const std::source_location& location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "guest_dynamic_loader_variants: failed at %s:%u\n", location.file_name(), location.line());
    std::abort();
}
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
