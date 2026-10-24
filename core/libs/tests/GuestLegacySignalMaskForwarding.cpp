#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sigblock_nid_postfix(int);
int APS5_VABI sigsetmask_nid_postfix(int);
}

namespace {

int observedHow = 0;
unsigned calls = 0;
std::array<std::uint32_t, 4> observedSet{};
std::uint32_t previousMask = 0;
int backendResult = 0;
int backendError = 0;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "legacy signal mask forwarding: %s\n", message);
        std::abort();
    }
}

using MaskFunction = int (APS5_VABI *)(int);

void Check(MaskFunction function, int how) {
    constexpr std::array<std::uint32_t, 7> masks = {0, 1, 0x10000, 0x7fffffff, 0x80000000, 0x87654321, 0xffffffff};
    for (const auto mask : masks) {
        for (const auto previous : masks) {
            previousMask = previous;
            backendResult = 0;
            calls = 0;
            errno = 123;
            const int result = function(std::bit_cast<int>(mask));
            Require(result == std::bit_cast<int>(previous), "the previous mask lost bits");
            Require(calls == 1 && observedHow == how, "wrong sigprocmask operation");
            Require(observedSet == std::array<std::uint32_t, 4>{mask, 0, 0, 0}, "the mask was not zero-extended to 128 bits");
            Require(errno == 123, "a successful call changed errno");
        }
    }
    for (const int error : {4, 12, 22}) {
        backendResult = -1;
        backendError = error;
        calls = 0;
        Require(function(-1) == -1, "a backend failure was not propagated");
        Require(calls == 1 && observedHow == how && errno == error, "the backend error was changed");
    }
}

}

extern "C" int APS5_VABI sigprocmask_nid_postfix(int how, const void* set, void* previousSet) {
    Require(set != nullptr && previousSet != nullptr, "missing signal set storage");
    ++calls;
    observedHow = how;
    std::memcpy(observedSet.data(), set, sizeof(observedSet));
    if (backendResult == 0) {
        const std::array<std::uint32_t, 4> previous = {previousMask, 0x12345678, 0x87654321, 0xffffffff};
        std::memcpy(previousSet, previous.data(), sizeof(previous));
    } else {
        errno = backendError;
    }
    return backendResult;
}

int main() {
    Check(sigblock_nid_postfix, 1);
    Check(sigsetmask_nid_postfix, 3);
}
