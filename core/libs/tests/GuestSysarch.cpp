#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall> static std::string Thrown(TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

#if defined(__linux__) && defined(__x86_64__)
static void* APS5_VABI ReadGsBase(void* arg) {
    auto* read = static_cast<std::uint64_t*>(arg);
    if (sysarch(130, read) != 0) *read = 0;
    return nullptr;
}

static void RequireLowHalf(int operation, std::uint64_t base) {
    unsigned char bytes[8];
    std::memset(bytes, 0xaa, sizeof(bytes));
    Require(sysarch(operation, bytes) == 0);
    std::uint32_t low = 0;
    std::memcpy(&low, bytes, sizeof(low));
    Require(low == static_cast<std::uint32_t>(base));
    for (int i = 4; i < 8; ++i) Require(bytes[i] == 0xaa);
}
#endif

int main() {
    Require(Thrown([] { sysarch(129, nullptr); }) == "sysarch: operation 129 not implemented");
    Require(Thrown([] { sysarch(132, nullptr); }) == "sysarch: operation 132 not implemented");
    for (const int operation : {2, 5, 6, 12, 127, 133, 134, 1000, -1}) {
        *__error_nid_postfix() = 0;
        Require(sysarch(operation, nullptr) == -1 && *__error_nid_postfix() == 22);
    }
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
    RequireLowHalf(7, fs);
    RequireLowHalf(9, base);
    *__error_nid_postfix() = 0;
    Require(sysarch(7, nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sysarch(9, nullptr) == -1 && *__error_nid_postfix() == 14);
    std::uint64_t inherited = 0;
    Pthread child = nullptr;
    Require(scePthreadCreate(&child, nullptr, ReadGsBase, &inherited, "sysarch_gs") == 0);
    Require(scePthreadJoin(child, nullptr) == 0 && inherited == base);
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
    Require(Thrown([&base] { sysarch(7, &base); }) == "sysarch: segment bases on this host not implemented");
#endif
    return 0;
}
