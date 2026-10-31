#include "prx/libSceGnmDriverForNeoMode/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "ProcessParametersFixture.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

constexpr std::uint32_t Untouched = 0xa5a5a5a5u;
constexpr std::array<std::uint32_t, 19> ContextStateBefore400{
    0xc0012800u, 0x80000000u, 0x80000000u, 0xc0001200u, 0x00000000u, 0xc0002f00u, 0x00000001u, 0xc0016900u,
    0x00000102u, 0x00000000u, 0xc0016900u, 0x00000202u, 0x00cc0010u, 0xc0026900u, 0x000000ebu, 0xff00ff00u,
    0x0000ff00u, 0xc00d1000u, 0x00000000u,
};
constexpr std::size_t BufferWords = 0x110;

using Buffer = std::array<std::uint32_t, BufferWords>;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "GnmNeo InternalCommand: %s\n", message);
    std::abort();
}

Buffer Fresh() {
    Buffer buffer;
    buffer.fill(Untouched);
    return buffer;
}

void CheckSize(std::uint32_t sdkVersion, int (APS5_VABI* expected)(std::uint32_t*, std::uint32_t), std::uint32_t size) {
    Buffer internal = Fresh();
    Buffer reference = Fresh();
    const int internalResult = sceGnmDrawInitToDefaultContextStateInternalCommand(internal.data(), size);
    const int referenceResult = expected(reference.data(), size);
    if (internalResult != referenceResult || internal != reference) {
        std::fprintf(stderr, "SDK 0x%08x, size 0x%x: %d against %d\n", sdkVersion, size, internalResult, referenceResult);
        Require(false, "the result or the written words differ from the matching context state call");
    }
}

}

int main() {
    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "the process parameters could not be installed");

    for (const std::uint32_t sdkVersion : {0x03500000u, 0x03ffffffu}) {
        std::memcpy(process.data() + 0x10, &sdkVersion, sizeof(sdkVersion));
        Buffer words = Fresh();
        Require(sceGnmDrawInitToDefaultContextStateInternalCommand(words.data(), 0x20) == 0x20, "size 0x20 was not taken before SDK 0x04000000");
        for (std::size_t index = 0; index < BufferWords; ++index)
            Require(words[index] == (index < ContextStateBefore400.size() ? ContextStateBefore400[index] : Untouched), "the words before SDK 0x04000000 differ from firmware 13.42's");
        for (const std::uint32_t size : {0u, 0x1fu, 0x21u, 0xffu, 0x100u, 0x101u}) {
            Buffer refused = Fresh();
            Require(sceGnmDrawInitToDefaultContextStateInternalCommand(refused.data(), size) == 0 && refused == Fresh(), "a size other than 0x20 was taken before SDK 0x04000000");
        }
    }

    for (const std::uint32_t sdkVersion : {0x04000000u, 0x09000000u}) {
        std::memcpy(process.data() + 0x10, &sdkVersion, sizeof(sdkVersion));
        Buffer words = Fresh();
        Require(sceGnmDrawInitToDefaultContextStateInternalCommand(words.data(), 0x100) == 0x100, "size 0x100 was not taken from SDK 0x04000000");
        std::size_t written = 0;
        while (written < BufferWords && words[written] != Untouched) ++written;
        Require(written == 102, "size 0x100 did not write 102 words from SDK 0x04000000");
        for (const std::uint32_t size : {0u, 0x20u, 0xffu, 0x100u, 0x101u}) CheckSize(sdkVersion, sceGnmDrawInitToDefaultContextState400, size);
    }

    std::puts("GnmNeo InternalCommand tests passed");
    return 0;
}
