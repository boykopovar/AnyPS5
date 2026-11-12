#include "prx/libSceZlib/include/Api.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool valid, int line) {
    if (valid) return;
    std::fprintf(stderr, "Zlib guest pointer check failed at line %d\n", line);
    std::abort();
}

#define Check(value) Require((value), __LINE__)

}

int main() {
    constexpr std::array<std::uint8_t, 77> compressed{0x78, 0x9c, 0xed, 0xca, 0xb1, 0xd, 0x80, 0x20, 0x10, 0x40, 0xd1, 0x9e, 0x29, 0x6e, 0x2, 0x3a, 0x6, 0x60, 0x3, 0x12, 0x26, 0x10, 0x30, 0x91, 0x88, 0x47, 0x71, 0xa1, 0xd0, 0xe9, 0x1d, 0x82, 0xf6, 0xbf, 0xfa, 0x45, 0x7d, 0x53, 0xe, 0xf2, 0x8d, 0x5e, 0xa4, 0x5e, 0x67, 0xbd, 0x6d, 0x3d, 0x72, 0x68, 0x93, 0x32, 0x97, 0x36, 0xf3, 0x2e, 0x12, 0x8, 0x4, 0x2, 0x81, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x81, 0x40, 0x20, 0xec, 0x85, 0x1f, 0xf5, 0xf3, 0xbd, 0x4c};
    const auto invalidSource = reinterpret_cast<const void*>(std::uintptr_t{0x10});
    const auto invalidDestination = reinterpret_cast<void*>(std::uintptr_t{0x1000});
    const auto invalidId = reinterpret_cast<std::uint64_t*>(std::uintptr_t{0x10});
    constexpr std::int32_t invalidArgument = static_cast<std::int32_t>(0x81120016);
    constexpr std::int32_t timedOut = static_cast<std::int32_t>(0x81120027);
    alignas(2048) std::array<std::uint8_t, 8192> output{};
    std::uint64_t requestId = 0x1122334455667788;

    Check(sceZlibInitialize(nullptr, 0) == 0);
    errno = EDOM;
    Check(sceZlibInflate(invalidSource, 1, output.data(), output.size(), &requestId) == invalidArgument);
    Check(requestId == 0x1122334455667788 && errno == EDOM);
    Check(sceZlibInflate(compressed.data(), compressed.size(), invalidDestination, output.size(), &requestId) == invalidArgument);
    Check(requestId == 0x1122334455667788 && errno == EDOM);
    Check(sceZlibInflate(compressed.data(), compressed.size(), output.data(), output.size(), invalidId) == invalidArgument);
    Check(requestId == 0x1122334455667788 && errno == EDOM);
    Check(requestId == 0x1122334455667788);
    std::uint64_t completed = 0;
    std::uint32_t timeout = 0;
    Check(sceZlibWaitForDone(&completed, &timeout) == timedOut);
    Check(sceZlibFinalize() == 0);
}
