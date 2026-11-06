#include "prx/libSceAgc/Misc/include/StaticBuffer.hpp"
#include "prx/libc/include/GuestArena.hpp"
#ifdef __APPLE__
#include "prx/libSceAgcDriver/Execution/include/Driver/DriverMemory.hpp"
#endif

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {

#ifdef __APPLE__
constexpr std::uintptr_t StaticTable = AgcDriver::DriverDetail::RetargetPs5DriverMemory(0xfe0040000ull) + GuestArena::GuestAddressOffset;
#else
constexpr std::uintptr_t StaticTable = 0xfe0040000ull + GuestArena::GuestAddressOffset;
#endif
constexpr int InvalidStaticBuffer = static_cast<int>(0x8a6c000bu);

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::array<std::uint64_t, 2> slot(std::uint32_t index) {
    std::array<std::uint64_t, 2> words{};
    std::memcpy(words.data(), reinterpret_cast<const void*>(StaticTable + index * 16u), sizeof(words));
    return words;
}

void testBuffers() {
    std::array<std::uint64_t, 2> fetchShaderTable{};
    check(sceAgcGetStaticBuffer(0, fetchShaderTable.data()) == 0, "reading static buffer 0 failed");
    check(fetchShaderTable == slot(0) && fetchShaderTable[0] >> 48u == 16u, "static buffer 0 is not the driver's fetch shader table");
    const std::array<std::uint64_t, 2> written{0x1122334455667788ull, 0x99aabbccddeeff00ull};
    check(sceAgcSetStaticBuffer(5, written.data()) == 0, "writing static buffer 5 failed");
    check(slot(5) == written, "static buffer 5 was not stored at the sixth 16-byte slot");
    std::array<std::uint64_t, 2> read{};
    check(sceAgcGetStaticBuffer(5, read.data()) == 0 && read == written, "static buffer 5 did not read back");
    check(sceAgcSetStaticBuffer(6, written.data()) == InvalidStaticBuffer, "static buffer index 6 was accepted");
    check(sceAgcGetStaticBuffer(6, read.data()) == InvalidStaticBuffer, "static buffer index 6 was read");
    check(slot(0) == fetchShaderTable, "static buffer writes changed slot 0");
}

void testQwords() {
    check(sceAgcSetStaticQword(0, 0x0123456789abcdefull) == 0, "writing static qword 0 failed");
    check(sceAgcSetStaticQword(3, 0xfedcba9876543210ull) == 0, "writing static qword 3 failed");
    check(slot(1)[0] == 0x0123456789abcdefull && slot(2)[1] == 0xfedcba9876543210ull, "static qwords are not stored after the first 16-byte slot");
    std::uint64_t value = 0;
    check(sceAgcGetStaticQword(3, &value) == 0 && value == 0xfedcba9876543210ull, "static qword 3 did not read back");
    std::array<std::uint64_t, 2> buffer{};
    check(sceAgcGetStaticBuffer(1, buffer.data()) == 0 && buffer[0] == 0x0123456789abcdefull, "static qword 0 is not the first half of static buffer 1");
}

}

int main() {
#ifdef ANYPS5_ENABLE_ASAN
    std::puts("skipped: the static table lives at the PS5's fixed 0xfe0040000, which an ANYPS5_ENABLE_ASAN build cannot map");
    return 0;
#endif
    try {
        testBuffers();
        testQwords();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
