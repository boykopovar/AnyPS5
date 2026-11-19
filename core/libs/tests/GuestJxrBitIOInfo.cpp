#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJxr/libSceJxr.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>

extern "C" {
void* JxrBitIOInfoFixtureCreate(int indexTable, int format, std::uint32_t vertical, std::uint32_t horizontal, int subband);
void JxrBitIOInfoFixtureDestroy(void* value);
int JxrBitIOInfoFixtureUnchanged(void* value);
std::size_t JxrBitIOInfoFixtureSize();
std::size_t JxrBitIOInfoFixtureBitIOSize();
std::size_t JxrBitIOInfoFixturePacketLength();
std::size_t JxrBitIOInfoFixtureMaxTiles();
std::size_t JxrBitIOInfoFixtureCount(void* value);
int JxrBitIOInfoFixtureSubbands(void* value);
void* JxrBitIOInfoFixtureArray(void* value);
void* JxrBitIOInfoFixtureIndex(void* value);
void* JxrBitIOInfoFixtureRing(void* value, std::size_t index);
std::size_t JxrBitIOInfoFixtureHeaderSize(void* value);
void* JxrBitIOInfoFixtureHeader(void* value);
std::size_t JxrBitIOInfoFixtureStructSize(void* value);
}

namespace {

void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "GuestJxrBitIOInfo.cpp:%d: requirement failed\n", line); std::abort(); } }
#define Require(value) Check((value), __LINE__)

std::array<void*, 8> allocations{};
std::array<std::size_t, 8> allocationSizes{};
std::size_t allocationCalls = 0;
std::size_t failAllocationCall = 0;

void* APS5_VABI Allocate(std::size_t size) {
    ++allocationCalls;
    allocations[allocationCalls - 1] = failAllocationCall == allocationCalls ? nullptr : std::malloc(size);
    allocationSizes[allocationCalls - 1] = size;
    return allocations[allocationCalls - 1];
}

void APS5_VABI Free(void* pointer) { std::free(pointer); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) { std::memcpy(data.data() + offset, &value, sizeof(value)); };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&Allocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&Free));
    write(replacement, 0x30, reinterpret_cast<void*>(&Calloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&Reallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&Align));
    write(replacement, 0x48, reinterpret_cast<void*>(&Realign));
    write(replacement, 0x50, reinterpret_cast<void*>(&PosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}

void ResetAllocations(std::size_t failAt = 0) {
    allocations.fill(nullptr);
    allocationSizes.fill(0);
    allocationCalls = 0;
    failAllocationCall = failAt;
}

std::size_t BlockSize(std::size_t count) {
    const auto packet = JxrBitIOInfoFixturePacketLength();
    return JxrBitIOInfoFixtureBitIOSize() * count + packet * 4 - 1 + packet * 4 * count;
}

void FreeRecorded() {
    for (std::size_t i = 0; i < allocationCalls; ++i) std::free(allocations[i]);
}

void RequireUnchangedFields(void* codec) {
    Require(JxrBitIOInfoFixtureUnchanged(codec));
    Require(JxrBitIOInfoFixtureHeaderSize(codec) == 0x5678);
    Require(JxrBitIOInfoFixtureHeader(codec) == reinterpret_cast<void*>(std::uintptr_t{0x1234}));
    Require(JxrBitIOInfoFixtureStructSize(codec) == 0x12345678);
}

void TestSuccessAndLayout() {
    constexpr int Spatial = 0;
    constexpr int Frequency = 1;
    constexpr int All = 0;
    constexpr int NoFlex = 1;
    constexpr int NoHighpass = 2;
    constexpr int DcOnly = 3;
    const auto packet = JxrBitIOInfoFixturePacketLength();
    const auto bitIoSize = JxrBitIOInfoFixtureBitIOSize();

    void* codec = JxrBitIOInfoFixtureCreate(1, Spatial, 1, 2, DcOnly);
    Require(codec != nullptr && JxrBitIOInfoFixtureSize() > sizeof(std::uint64_t));
    ResetAllocations();
    Require(allocateBitIOInfo_nid_postfix(codec) == 0);
    Require(allocationCalls == 2 && allocationSizes[0] == BlockSize(2));
    Require(allocationSizes[1] == 2 * 3 * sizeof(std::size_t));
    Require(JxrBitIOInfoFixtureCount(codec) == 2 && JxrBitIOInfoFixtureSubbands(codec) == 1);
    Require(JxrBitIOInfoFixtureArray(codec) == allocations[0]);
    Require(JxrBitIOInfoFixtureIndex(codec) == allocations[1]);
    auto expectedRing = (reinterpret_cast<std::uintptr_t>(allocations[0]) + bitIoSize * 2 + packet * 4 - 1) & ~(static_cast<std::uintptr_t>(packet * 4) - 1);
    expectedRing += packet * 2;
    for (auto address = reinterpret_cast<std::uintptr_t>(allocations[0]) + bitIoSize * 2;
         address < expectedRing - packet * 2; ++address)
        Require(*reinterpret_cast<const std::uint8_t*>(address) == 0);
    for (std::size_t i = 0; i < 2; ++i) {
        Require(JxrBitIOInfoFixtureRing(codec, i) == reinterpret_cast<void*>(expectedRing + i * packet * 4));
        const auto* bytes = static_cast<const std::uint8_t*>(JxrBitIOInfoFixtureRing(codec, i)) - packet * 2;
        for (std::size_t j = 0; j < packet * 4; ++j) Require(bytes[j] == 0);
    }
    Require(JxrBitIOInfoFixtureHeaderSize(codec) == 0x5678);
    Require(JxrBitIOInfoFixtureHeader(codec) == reinterpret_cast<void*>(std::uintptr_t{0x1234}));
    Require(JxrBitIOInfoFixtureStructSize(codec) == 0x12345678);
    RequireUnchangedFields(codec);
    FreeRecorded();
    JxrBitIOInfoFixtureDestroy(codec);

    const std::array<std::pair<int, int>, 4> subbands{{{All, 4}, {NoFlex, 3}, {NoHighpass, 2}, {DcOnly, 1}}};
    for (const auto [subband, expected] : subbands) {
        codec = JxrBitIOInfoFixtureCreate(1, Frequency, 0, 0, subband);
        ResetAllocations();
        Require(allocateBitIOInfo_nid_postfix(codec) == 0);
        Require(JxrBitIOInfoFixtureSubbands(codec) == expected && JxrBitIOInfoFixtureCount(codec) == static_cast<std::size_t>(expected));
        Require(allocationCalls == 2 && allocationSizes[0] == BlockSize(expected));
        Require(allocationSizes[1] == static_cast<std::size_t>(expected) * sizeof(std::size_t));
        FreeRecorded();
        JxrBitIOInfoFixtureDestroy(codec);
    }

    codec = JxrBitIOInfoFixtureCreate(1, Frequency, 1, 0, All);
    ResetAllocations();
    Require(allocateBitIOInfo_nid_postfix(codec) == 0);
    Require(JxrBitIOInfoFixtureCount(codec) == 8 && allocationSizes[1] == 8 * sizeof(std::size_t));
    RequireUnchangedFields(codec);
    FreeRecorded();
    JxrBitIOInfoFixtureDestroy(codec);

    codec = JxrBitIOInfoFixtureCreate(0, Spatial, 0, 0, NoHighpass);
    ResetAllocations();
    Require(allocateBitIOInfo_nid_postfix(codec) == 0);
    Require(allocationCalls == 0 && JxrBitIOInfoFixtureCount(codec) == 0);
    Require(JxrBitIOInfoFixtureSubbands(codec) == 2);
    Require(JxrBitIOInfoFixtureArray(codec) == reinterpret_cast<void*>(std::uintptr_t{0x3456}));
    Require(JxrBitIOInfoFixtureIndex(codec) == reinterpret_cast<void*>(std::uintptr_t{0x2345}));
    JxrBitIOInfoFixtureDestroy(codec);
}

void TestErrorsAndPartialState() {
    constexpr int Spatial = 0;
    constexpr int Frequency = 1;
    constexpr int All = 0;
    const auto oldArray = reinterpret_cast<void*>(std::uintptr_t{0x3456});
    const auto oldIndex = reinterpret_cast<void*>(std::uintptr_t{0x2345});

    void* codec = JxrBitIOInfoFixtureCreate(1, Spatial, 0, 0, All);
    ResetAllocations(1);
    Require(allocateBitIOInfo_nid_postfix(codec) == -1);
    Require(allocationCalls == 1 && allocationSizes[0] == BlockSize(1));
    Require(JxrBitIOInfoFixtureSubbands(codec) == 4 && JxrBitIOInfoFixtureArray(codec) == oldArray);
    Require(JxrBitIOInfoFixtureIndex(codec) == oldIndex && JxrBitIOInfoFixtureCount(codec) == 0x4567);
    RequireUnchangedFields(codec);
    JxrBitIOInfoFixtureDestroy(codec);

    codec = JxrBitIOInfoFixtureCreate(1, Spatial, 0, 0, All);
    ResetAllocations(2);
    Require(allocateBitIOInfo_nid_postfix(codec) == -1);
    Require(allocationCalls == 2 && allocationSizes[0] == BlockSize(1) && allocationSizes[1] == sizeof(std::size_t));
    Require(JxrBitIOInfoFixtureArray(codec) == allocations[0] && JxrBitIOInfoFixtureIndex(codec) == nullptr);
    Require(JxrBitIOInfoFixtureCount(codec) == 0x4567);
    RequireUnchangedFields(codec);
    RequireUnchangedFields(codec);
    FreeRecorded();
    JxrBitIOInfoFixtureDestroy(codec);

    codec = JxrBitIOInfoFixtureCreate(1, Spatial, 0, static_cast<std::uint32_t>(JxrBitIOInfoFixtureMaxTiles()), All);
    ResetAllocations();
    Require(allocateBitIOInfo_nid_postfix(codec) == -1);
    Require(allocationCalls == 1 && JxrBitIOInfoFixtureArray(codec) == allocations[0]);
    Require(JxrBitIOInfoFixtureIndex(codec) == oldIndex && JxrBitIOInfoFixtureCount(codec) == 0x4567);
    RequireUnchangedFields(codec);
    RequireUnchangedFields(codec);
    FreeRecorded();
    JxrBitIOInfoFixtureDestroy(codec);

    codec = JxrBitIOInfoFixtureCreate(1, Frequency, static_cast<std::uint32_t>(JxrBitIOInfoFixtureMaxTiles() * 4), 0, All);
    ResetAllocations();
    Require(allocateBitIOInfo_nid_postfix(codec) == -1);
    Require(allocationCalls == 0 && JxrBitIOInfoFixtureSubbands(codec) == 4);
    Require(JxrBitIOInfoFixtureArray(codec) == oldArray && JxrBitIOInfoFixtureIndex(codec) == oldIndex);
    Require(JxrBitIOInfoFixtureCount(codec) == 0x4567);
    RequireUnchangedFields(codec);
    JxrBitIOInfoFixtureDestroy(codec);

    bool threw = false;
    try { allocateBitIOInfo_nid_postfix(nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

}

int main() {
    InitializeHeap();
    TestSuccessAndLayout();
    TestErrorsAndPartialState();
    return 0;
}
