#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

#if (defined(__linux__) || defined(_WIN32)) && defined(__x86_64__)
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

struct ThreadBases {
    std::uint64_t expected;
    std::uint64_t replacement;
};

static void* APS5_VABI ChangeGsBase(void* arg) {
    auto& bases = *static_cast<ThreadBases*>(arg);
    std::uint64_t read = 0;
    Require(sysarch(130, &read) == 0 && read == bases.expected);
    Require(sysarch(131, &bases.replacement) == 0);
    Require(sysarch(130, &read) == 0 && read == bases.replacement);
    RequireLowHalf(9, read);
    Pthread nested = nullptr;
    std::uint64_t inherited = 0;
    Require(pthread_create_nid_postfix(&nested, nullptr, ReadGsBase, &inherited) == 0);
    Require(pthread_join_nid_postfix(nested, nullptr) == 0 && inherited == bases.replacement);
    return nullptr;
}
#endif

int main() {
    for (const int operation : {2, 5, 6, 12, 127, 133, 134, 1000, -1}) {
        *__error_nid_postfix() = 0;
        Require(sysarch(operation, nullptr) == -1 && *__error_nid_postfix() == 22);
    }
    for (const int operation : {3, 4, 11, 132}) {
        *__error_nid_postfix() = 0;
        Require(sysarch(operation, nullptr) == -1 && *__error_nid_postfix() == 14);
    }
    struct {
        std::uint32_t start;
        std::uint32_t descsLow;
        std::uint32_t descsHigh;
        std::uint32_t num;
    } ldt{};
    static_assert(sizeof(ldt) == 16);
    static std::uint64_t descriptor = 0;
    for (const int operation : {0, 1}) {
        for (void* const argument : {static_cast<void*>(&ldt), static_cast<void*>(nullptr), reinterpret_cast<void*>(std::uintptr_t{0x10})}) {
            for (const std::uint32_t count : {0u, 4u, 513u}) {
                ldt = {};
                ldt.start = count == 0 ? 3 : 0;
                ldt.num = count;
                const auto address = reinterpret_cast<std::uintptr_t>(&descriptor);
                ldt.descsLow = static_cast<std::uint32_t>(address);
                ldt.descsHigh = static_cast<std::uint32_t>(address >> 32);
                *__error_nid_postfix() = 0;
                Require(sysarch(operation, argument) == -1 && *__error_nid_postfix() == 22);
            }
        }
    }
    std::uint32_t ports[3] = {0x60, 5, 1};
    Require(sysarch(3, ports) == 0 && ports[0] == 0x60 && ports[1] == 0 && ports[2] == 1);
    std::uint32_t outsidePorts[3] = {65536, 5, 1};
    *__error_nid_postfix() = 0;
    Require(sysarch(3, outsidePorts) == -1 && *__error_nid_postfix() == 22 && outsidePorts[1] == 5);
    *__error_nid_postfix() = 0;
    Require(sysarch(4, ports) == -1 && *__error_nid_postfix() == 1);
#if (defined(__linux__) || defined(_WIN32)) && defined(__x86_64__)
    static std::uint64_t slots[2] = {0x1234567890abcdefull, 0};
    std::uint64_t base = reinterpret_cast<std::uintptr_t>(slots);
    Require(sysarch(131, &base) == 0);
    std::uint64_t read = 0;
    Require(sysarch(130, &read) == 0 && read == base);
#ifdef __linux__
    std::uint64_t through = 0;
    asm volatile("movq %%gs:0, %0" : "=r"(through));
    Require(through == slots[0]);
    std::uint64_t fs = 0;
    Require(sysarch(128, &fs) == 0 && fs != 0);
    std::uint64_t self = 0;
    asm volatile("movq %%fs:0, %0" : "=r"(self));
    Require(self == fs);
    RequireLowHalf(7, fs);
    *__error_nid_postfix() = 0;
    Require(sysarch(7, nullptr) == -1 && *__error_nid_postfix() == 14);
#endif
    RequireLowHalf(9, base);
    *__error_nid_postfix() = 0;
    Require(sysarch(9, nullptr) == -1 && *__error_nid_postfix() == 14);
    void* const bad = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10));
#ifdef __linux__
    *__error_nid_postfix() = 0;
    Require(sysarch(7, bad) == -1 && *__error_nid_postfix() == 14);
#endif
    *__error_nid_postfix() = 0;
    Require(sysarch(9, bad) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sysarch(128, nullptr) == -1 && *__error_nid_postfix() == 14);
#ifdef __linux__
    *__error_nid_postfix() = 0;
    Require(sysarch(128, bad) == -1 && *__error_nid_postfix() == 14);
#endif
    *__error_nid_postfix() = 0;
    Require(sysarch(130, bad) == -1 && *__error_nid_postfix() == 14);
    std::uint64_t inherited = 0;
    Pthread child = nullptr;
    Require(scePthreadCreate(&child, nullptr, ReadGsBase, &inherited, "sysarch_gs") == 0);
    Require(scePthreadJoin(child, nullptr) == 0 && inherited == base);
    ThreadBases bases{base, 0x0000123456789abcull};
    Require(scePthreadCreate(&child, nullptr, ChangeGsBase, &bases, "sysarch_change") == 0);
    Require(scePthreadJoin(child, nullptr) == 0);
    Require(sysarch(130, &read) == 0 && read == base);
    Require(pthread_create_nid_postfix(&child, nullptr, ChangeGsBase, &bases) == 0);
    Require(pthread_join_nid_postfix(child, nullptr) == 0);
    Require(sysarch(130, &read) == 0 && read == base);
    std::uint64_t outside = 0x0000800000000000ull;
    *__error_nid_postfix() = 0;
    Require(sysarch(131, &outside) == -1 && *__error_nid_postfix() == 22);
    Require(sysarch(130, &read) == 0 && read == base);
    *__error_nid_postfix() = 0;
    Require(sysarch(131, nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sysarch(131, bad) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sysarch(130, nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(sysarch(131, reinterpret_cast<void*>(1)) == -1 && *__error_nid_postfix() == 14);
    Require(sysarch(130, reinterpret_cast<void*>(1)) == -1 && *__error_nid_postfix() == 14);
    Require(sysarch(9, reinterpret_cast<void*>(1)) == -1 && *__error_nid_postfix() == 14);
    Require(sysarch(130, &read) == 0 && read == base);
    std::uint32_t low = 0x12345678;
    Require(sysarch(10, &low) == 0 && sysarch(130, &read) == 0 && read == 0x12345678);
    *__error_nid_postfix() = 0;
    Require(sysarch(10, nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(sysarch(131, &base) == 0 && sysarch(130, &read) == 0 && read == base);
    std::uint64_t zero = 0;
    Require(sysarch(131, &zero) == 0 && sysarch(130, &read) == 0 && read == 0);
#endif
    return 0;
}
