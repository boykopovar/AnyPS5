#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>

static constexpr unsigned DefaultMaskCpus = 13;

static bool Check(bool value, const char* what) {
    if (!value) std::fprintf(stderr, "%s\n", what);
    return value;
}

int main() {
    if (!Check(_ZNSt6thread20hardware_concurrencyEv_nid_postfix() == DefaultMaskCpus, "hardware_concurrency is not the CPU count of the default mask")) return 1;
    Pthread self = scePthreadSelf();
    KernelCpumask original = 0;
    if (!Check(scePthreadGetaffinity(self, &original) == 0, "scePthreadGetaffinity failed")) return 1;
    if (!Check(scePthreadSetaffinity(self, 0x2) == 0, "scePthreadSetaffinity failed")) return 1;
    const unsigned narrowed = _ZNSt6thread20hardware_concurrencyEv_nid_postfix();
    if (!Check(scePthreadSetaffinity(self, original) == 0, "scePthreadSetaffinity could not restore the mask")) return 1;
    return Check(narrowed == DefaultMaskCpus, "hardware_concurrency followed the calling thread's mask") ? 0 : 1;
}
