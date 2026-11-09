#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int main() {
    constexpr std::size_t page = 0x4000;
    void* memory = nullptr;
    Require(sceKernelMapFlexibleMemory(&memory, page, 3, 0) == 0 && memory != nullptr, "failed to map flexible memory");
    Require(sceKernelMprotect(memory, page, 1) == 0, "failed to mprotect memory read-only");
    
    const auto readOnly = static_cast<const GuestSignalSet*>(memory);
    const auto unreadable = reinterpret_cast<const GuestSignalSet*>(std::uintptr_t{0x10});

    auto test_sigsuspend = [](const GuestSignalSet* mask, const char* desc) {
        int res = sigsuspend_nid_postfix(mask);
        int err = *__error_nid_postfix();
        Require(res == -1, desc);
        Require(err == 14, desc);
    };

    auto test_underscore_sigsuspend = [](const GuestSignalSet* mask, const char* desc) {
        int res = _sigsuspend_nid_postfix(mask);
        int err = *__error_nid_postfix();
        Require(res == -1, desc);
        Require(err == 14, desc);
    };

    test_sigsuspend(nullptr, "sigsuspend(nullptr) must return EFAULT (14)");
    test_sigsuspend(unreadable, "sigsuspend(unreadable) must return EFAULT (14)");
    test_sigsuspend(readOnly, "sigsuspend(readOnly) must return EFAULT (14)");

    test_underscore_sigsuspend(nullptr, "_sigsuspend(nullptr) must return EFAULT (14)");
    test_underscore_sigsuspend(unreadable, "_sigsuspend(unreadable) must return EFAULT (14)");
    test_underscore_sigsuspend(readOnly, "_sigsuspend(readOnly) must return EFAULT (14)");

    std::printf("ALL SIGSUSPEND NULL MASK TESTS PASSED\n");
    return 0;
}
