#include "prx/libSceAvSetting/include/Api.hpp"
#include "prx/libSceAvSetting/include/InventoryExports.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "AVSETTING PREDICATES: %s\n", message);
    std::abort();
}

void Write(void* destination, const void* source, std::size_t bytes) {
    const GuestArena::HostWrite access(destination, bytes);
    Require(access.Open(), "guest fixture memory was not writable");
    std::memcpy(destination, source, bytes);
}

template <typename TValue>
void WriteField(void* object, std::size_t offset, TValue value) {
    Write(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

void* Allocate(std::size_t bytes) {
    void* memory = GuestHeap::GuestHeapAllocate_nid_postfix(bytes);
    Require(memory != nullptr, "guest fixture allocation failed");
    const std::array<std::byte, 0x100> zeros{};
    auto* destination = static_cast<std::byte*>(memory);
    for (std::size_t offset = 0; offset < bytes; offset += zeros.size()) {
        const auto count = bytes - offset < zeros.size() ? bytes - offset : zeros.size();
        Write(destination + offset, zeros.data(), count);
    }
    return memory;
}

void TestColorimetry() {
    Require(!sceAvControlIsColorimetryHdr(8), "colorimetry below the table range was accepted");
    for (std::uint32_t value = 9; value <= 20; ++value) {
        Require(sceAvControlIsColorimetryHdr(value) == (value != 18), "colorimetry table result is wrong");
    }
    Require(!sceAvControlIsColorimetryHdr(21), "colorimetry above the table range was accepted");
}

void TestDeepColor(void* monitorInfo) {
    Require(!sceAvControlIsDeepColorSupportedByMonitorInfo(nullptr), "null monitor info was accepted");
    Require(!sceAvControlIsDeepColorSupportedByMonitorInfo(reinterpret_cast<void*>(std::uintptr_t{1})),
            "unmapped monitor info was accepted");
    Require(!sceAvControlIsDeepColorSupportedByMonitorInfo(monitorInfo), "empty monitor info was accepted");

    WriteField(monitorInfo, 0x74, std::uint8_t{0x02});
    Require(sceAvControlIsDeepColorSupportedByMonitorInfo(monitorInfo), "first deep-color mask was ignored");
    WriteField(monitorInfo, 0x74, std::uint8_t{0});
    WriteField(monitorInfo, 0x75, std::uint8_t{0x08});
    Require(sceAvControlIsDeepColorSupportedByMonitorInfo(monitorInfo), "second deep-color mask was ignored");
    WriteField(monitorInfo, 0x75, std::uint8_t{0});
    WriteField(monitorInfo, 0x352, std::uint8_t{0x02});
    Require(sceAvControlIsDeepColorSupportedByMonitorInfo(monitorInfo), "extended deep-color mask was ignored");
    WriteField(monitorInfo, 0x352, std::uint8_t{0x04});
    Require(!sceAvControlIsDeepColorSupportedByMonitorInfo(monitorInfo), "unrelated extended bits were accepted");
}

void TestHdr(void* monitorInfo) {
    Require(!sceAvControlIsHdrSupportedByMonitorInfo(nullptr), "null monitor info was accepted");
    Require(!sceAvControlIsHdrSupportedByMonitorInfo(reinterpret_cast<void*>(std::uintptr_t{1})),
            "unmapped monitor info was accepted");
    Require(!sceAvControlIsHdrSupportedByMonitorInfo(monitorInfo), "empty monitor info was accepted");

    WriteField(monitorInfo, 0x78, std::uint8_t{0x10});
    Require(!sceAvControlIsHdrSupportedByMonitorInfo(monitorInfo), "HDR was accepted without the capability bit");
    WriteField(monitorInfo, 0x358, std::uint8_t{0x04});
    Require(sceAvControlIsHdrSupportedByMonitorInfo(monitorInfo), "HDR flags were not combined correctly");
    WriteField(monitorInfo, 0x78, std::uint8_t{0x40});
    Require(!sceAvControlIsHdrSupportedByMonitorInfo(monitorInfo), "unrelated HDR flags were accepted");
}

void TestModeFlags(void* videoOutMode) {
    Require(!sceAvControlIsModeGen1Vr(nullptr), "null video mode was accepted by Gen1 VR query");
    Require(!sceAvControlIsModeSystemDefault(nullptr), "null video mode was accepted by default-mode query");
    Require(!sceAvControlIsModeGen1Vr(reinterpret_cast<void*>(std::uintptr_t{1})),
            "unmapped video mode was accepted by Gen1 VR query");
    Require(!sceAvControlIsModeSystemDefault(reinterpret_cast<void*>(std::uintptr_t{1})),
            "unmapped video mode was accepted by default-mode query");
    Require(sceAvControlIsModeSystemDefault(videoOutMode), "clear system-default bit was not recognized");
    Require(!sceAvControlIsModeGen1Vr(videoOutMode), "clear Gen1 VR bit was accepted");

    WriteField(videoOutMode, 0x30, std::uint32_t{0x02});
    Require(sceAvControlIsModeGen1Vr(videoOutMode), "Gen1 VR bit was ignored");
    Require(sceAvControlIsModeSystemDefault(videoOutMode), "unrelated Gen1 VR bit changed default-mode result");
    WriteField(videoOutMode, 0x30, std::uint32_t{0x03});
    Require(!sceAvControlIsModeSystemDefault(videoOutMode), "set system-default bit was accepted");
}

void TestModeBit8(void* videoOutMode) {
    constexpr std::array<std::uint32_t, 6> Flags{0x0, 0x100, 0xff, 0xfffffeff, 0xffffffff, 0x80000101};
    for (const std::uint32_t flags : Flags) {
        WriteField(videoOutMode, 0x30, flags);
        Require(libSceAvSettingUnknown_kvYuLJa0CyU_nid_no_patch(videoOutMode) == static_cast<int>((flags >> 8) & 1),
                "bit 8 of the mode flags was not returned");
    }
}

}

int main() {
    auto* monitorInfo = Allocate(0x360);
    auto* videoOutMode = Allocate(0x34);
    TestColorimetry();
    TestDeepColor(monitorInfo);
    TestHdr(monitorInfo);
    TestModeFlags(videoOutMode);
    TestModeBit8(videoOutMode);
    std::puts("AvSetting predicate tests passed");
    return 0;
}
