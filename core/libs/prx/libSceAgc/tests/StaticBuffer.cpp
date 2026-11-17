#include "prx/libSceAgc/Misc/include/StaticBuffer.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "Testing.hpp"
#ifdef __APPLE__
#include "prx/libSceAgcDriver/Execution/include/Driver/DriverMemory.hpp"
#endif

#include <array>
#include <cerrno>
#include <cstddef>
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

std::array<std::uint64_t, 2> slot(std::uint32_t index) {
    std::array<std::uint64_t, 2> words{};
    std::memcpy(words.data(), reinterpret_cast<const void*>(StaticTable + index * 16u), sizeof(words));
    return words;
}

void testBuffers() {
    std::array<std::uint64_t, 2> fetchShaderTable{};
    Require(sceAgcGetStaticBuffer(0, fetchShaderTable.data()) == 0, "reading static buffer 0 failed");
    Require(fetchShaderTable == slot(0) && fetchShaderTable[0] >> 48u == 16u, "static buffer 0 is not the driver's fetch shader table");
    const std::array<std::uint64_t, 2> written{0x1122334455667788ull, 0x99aabbccddeeff00ull};
    Require(sceAgcSetStaticBuffer(5, written.data()) == 0, "writing static buffer 5 failed");
    Require(slot(5) == written, "static buffer 5 was not stored at the sixth 16-byte slot");
    std::array<std::uint64_t, 2> read{};
    Require(sceAgcGetStaticBuffer(5, read.data()) == 0 && read == written, "static buffer 5 did not read back");
    Require(sceAgcSetStaticBuffer(6, written.data()) == InvalidStaticBuffer, "static buffer index 6 was accepted");
    Require(sceAgcGetStaticBuffer(6, read.data()) == InvalidStaticBuffer, "static buffer index 6 was read");
    Require(slot(0) == fetchShaderTable, "static buffer writes changed slot 0");
}

void testInvalidGuestPointers() {
    const auto before = slot(2);
    errno = E2BIG;
    Require(sceAgcSetStaticBuffer(2, nullptr) == InvalidStaticBuffer, "static buffer set accepted a null guest pointer");
    Require(errno == E2BIG, "a rejected static buffer set changed errno");
    Require(sceAgcSetStaticBuffer(2, reinterpret_cast<const void*>(0x1)) == InvalidStaticBuffer, "static buffer set accepted an unmapped guest pointer");
    Require(slot(2) == before, "rejected static buffer set changed slot 2");
    std::array<std::uint8_t, 16> out{};
    out.fill(0xAA);
    errno = E2BIG;
    Require(sceAgcGetStaticBuffer(2, nullptr) == InvalidStaticBuffer, "static buffer get accepted a null guest pointer");
    Require(errno == E2BIG, "a rejected static buffer get changed errno");
    Require(sceAgcGetStaticBuffer(2, reinterpret_cast<void*>(0x1)) == InvalidStaticBuffer, "static buffer get accepted an unmapped guest pointer");
    for (std::uint8_t byte : out) Require(byte == 0xAA, "rejected static buffer get touched the sentinel output");
}

void testReadOnlyGuestPages() {
    auto* page = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(65536u, 4096u));
    Require(page != nullptr, "cannot reserve a read-only guest page fixture");
    GuestArena::GuestArenaCommit_nid_postfix(page, 65536u, 0x02, 4096u);
    const auto before = slot(1);
    errno = E2BIG;
    Require(sceAgcGetStaticBuffer(1, page) == InvalidStaticBuffer, "static buffer get wrote into a read-only guest page");
    Require(errno == E2BIG, "a rejected read-only static buffer get changed errno");
    GuestArena::GuestArenaReset_nid_postfix(page, 65536u);
    GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(page), 65536u);
    GuestArena::GuestArenaRelease_nid_postfix(page, 65536u);
    Require(slot(1) == before, "rejected static buffer get changed slot 1");
}

void testQwords() {
    Require(sceAgcSetStaticQword(0, 0x0123456789abcdefull) == 0, "writing static qword 0 failed");
    Require(sceAgcSetStaticQword(3, 0xfedcba9876543210ull) == 0, "writing static qword 3 failed");
    Require(slot(1)[0] == 0x0123456789abcdefull && slot(2)[1] == 0xfedcba9876543210ull, "static qwords are not stored after the first 16-byte slot");
    std::uint64_t value = 0;
    Require(sceAgcGetStaticQword(3, &value) == 0 && value == 0xfedcba9876543210ull, "static qword 3 did not read back");
    std::array<std::uint64_t, 2> buffer{};
    Require(sceAgcGetStaticBuffer(1, buffer.data()) == 0 && buffer[0] == 0x0123456789abcdefull, "static qword 0 is not the first half of static buffer 1");
    std::uint64_t routine = 0;
    std::memcpy(&routine, reinterpret_cast<const void*>(StaticTable + 96u), sizeof(routine));
    Require(sceAgcSetStaticQword(9, 0x5a5a5a5a5a5a5a5aull) == 0, "static qword 9, the last one inside the table, was rejected");
    Require(sceAgcSetStaticQword(10, 0xa5a5a5a5a5a5a5a5ull) == InvalidStaticBuffer, "static qword index 10 was accepted");
    Require(sceAgcGetStaticQword(10, &value) == InvalidStaticBuffer, "static qword index 10 was read");
    Require(sceAgcSetStaticQword(0xffffffffu, 0) == InvalidStaticBuffer, "static qword index 0xffffffff was accepted");
    std::uint64_t after = 0;
    std::memcpy(&after, reinterpret_cast<const void*>(StaticTable + 96u), sizeof(after));
    Require(after == routine, "rejected static qword writes changed the FS routine table");
}

}

int main() {
#ifdef ANYPS5_ENABLE_ASAN
    std::puts("skipped: the static table lives at the PS5's fixed 0xfe0040000, which an ANYPS5_ENABLE_ASAN build cannot map");
    return 0;
#endif
    try {
        testBuffers();
        testInvalidGuestPointers();
        testReadOnlyGuestPages();
        testQwords();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
