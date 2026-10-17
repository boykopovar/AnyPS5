#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sysarch(int number, void* args);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall> static std::string Thrown(TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

int main() {
    Require(Thrown([] { sysarch(129, nullptr); }) == "sysarch: operation 129 not implemented");
    Require(Thrown([] { sysarch(7, nullptr); }) == "sysarch: operation 7 not implemented");
#if defined(__linux__) && defined(__x86_64__)
    static std::uint64_t slots[2] = {0x1234567890abcdefull, 0};
    std::uint64_t base = reinterpret_cast<std::uintptr_t>(slots);
    Require(sysarch(131, &base) == 0);
    std::uint64_t read = 0;
    Require(sysarch(130, &read) == 0 && read == base);
    std::uint64_t through = 0;
    asm volatile("movq %%gs:0, %0" : "=r"(through));
    Require(through == slots[0]);
    std::uint64_t fs = 0;
    Require(sysarch(128, &fs) == 0 && fs != 0);
    std::uint64_t self = 0;
    asm volatile("movq %%fs:0, %0" : "=r"(self));
    Require(self == fs);
    std::uint64_t outside = 0x0000800000000000ull;
    *__error_nid_postfix() = 0;
    Require(sysarch(131, &outside) == -1 && *__error_nid_postfix() == 22);
    Require(sysarch(130, &read) == 0 && read == base);
    *__error_nid_postfix() = 0;
    Require(sysarch(131, nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(sysarch(130, nullptr) == -1 && *__error_nid_postfix() == 14);
    std::uint64_t zero = 0;
    Require(sysarch(131, &zero) == 0 && sysarch(130, &read) == 0 && read == 0);
#else
    std::uint64_t base = 0;
    Require(Thrown([&base] { sysarch(130, &base); }) == "sysarch: segment bases on this host not implemented");
#endif
    return 0;
}
