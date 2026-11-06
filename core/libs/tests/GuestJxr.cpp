#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <stdexcept>

extern "C" {
void* JxrFreeTileFixtureReset(std::uint32_t mode, std::int32_t subband, std::uint32_t lastTile, int empty);
void* JxrFreeTileFixtureSlot(std::size_t tile, std::size_t band);
void* JxrFreeTileFixtureTiles();
int JxrFreeTileFixtureIntact();
void JxrFreeTileFixtureClearTiles();
}

namespace {

void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "GuestJxr.cpp:%d: requirement failed\n", line); std::abort(); } }
#define Require(value) Check((value), __LINE__)

bool recordTileFrees = false;
std::array<void*, 16> tileFrees{};
std::size_t tileFreeCount = 0;
void* lastFreed = nullptr;
int frees = 0;
bool failAllocation = false;
std::size_t lastCount = 0;
std::size_t lastSize = 0;
void* lastAllocated = nullptr;
int callocCalls = 0;

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) {
    if (recordTileFrees) {
        Require(tileFreeCount < tileFrees.size());
        tileFrees[tileFreeCount++] = pointer;
        return;
    }
    if (pointer != nullptr) {
        lastFreed = pointer;
        ++frees;
    }
    std::free(pointer);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    ++callocCalls;
    lastCount = count;
    lastSize = size;
    lastAllocated = failAllocation ? nullptr : std::calloc(count, size);
    return lastAllocated;
}
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
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
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

}

void TestFreeTileInfo() {
    bool threw = false;
    try { freeTileInfo_nid_postfix(nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    recordTileFrees = true;
    for (std::uint32_t mode = 0; mode < 16; ++mode) {
        for (std::int32_t band = 0; band <= 4; ++band) {
            for (std::uint32_t lastTile = 0; lastTile < 3; ++lastTile) {
                for (int empty : {0, 1}) {
                    auto* codec = JxrFreeTileFixtureReset(mode, band, lastTile, empty);
                    std::array<void*, 16> expected{};
                    std::size_t count = 0;
                    const std::size_t bands = band == 3 ? 1 : band == 2 ? 2 : 3;
                    for (std::size_t current = 0; current < bands; ++current) {
                        const auto end = mode & (1U << current) ? lastTile : 0;
                        for (std::size_t tile = 0; tile <= end; ++tile) {
                            if (auto* pointer = JxrFreeTileFixtureSlot(tile, current)) expected[count++] = pointer;
                        }
                    }
                    expected[count++] = JxrFreeTileFixtureTiles();
                    tileFreeCount = 0;
                    freeTileInfo_nid_postfix(codec);
                    Require(tileFreeCount == count);
                    for (std::size_t i = 0; i < count; ++i) Require(tileFrees[i] == expected[i]);
                    Require(JxrFreeTileFixtureIntact());
                }
            }
        }
    }
    auto* codec = JxrFreeTileFixtureReset(0, 3, 0, 0);
    JxrFreeTileFixtureClearTiles();
    tileFreeCount = 0;
    threw = false;
    try { freeTileInfo_nid_postfix(codec); }
    catch (const std::exception&) { threw = true; }
    Require(threw && tileFreeCount == 0);
    recordTileFrees = false;
}

int main() {
    InitializeHeap();
    void* memory = malloc_nid_postfix(64);
    Require(memory != nullptr);
    void* const allocated = memory;
    Require(PKFree_nid_postfix(&memory) == 0);
    Require(memory == nullptr);
    Require(frees == 1 && lastFreed == allocated);
    Require(PKFree_nid_postfix(&memory) == 0);
    Require(memory == nullptr);
    Require(frees == 1);
    Require(PKFree_nid_postfix(nullptr) == 0);
    Require(frees == 1);
    for (const auto size : {std::size_t{1}, std::size_t{257}, std::size_t{4097}, std::size_t{0}}) {
        memory = &memory;
        const auto calls = callocCalls;
        const auto result = PKAlloc_nid_postfix(&memory, size);
        Require(callocCalls == calls + 1 && lastCount == 1 && lastSize == size);
        Require(memory == lastAllocated);
        Require(result == (memory ? 0 : -101));
        if (size != 0) Require(memory != nullptr);
        auto* bytes = static_cast<unsigned char*>(memory);
        for (std::size_t i = 0; i < size; ++i) Require(bytes[i] == 0);
        if (size != 0) std::memset(memory, 0xa5, size);
        const auto previousFrees = frees;
        const auto pointer = memory;
        Require(PKFree_nid_postfix(&memory) == 0 && memory == nullptr);
        Require(frees == previousFrees + (pointer ? 1 : 0));
        if (pointer) Require(lastFreed == pointer);
    }
    failAllocation = true;
    memory = &memory;
    Require(PKAlloc_nid_postfix(&memory, 37) == -101);
    Require(memory == nullptr && lastCount == 1 && lastSize == 37);
    failAllocation = false;
    bool threw = false;
    try { PKAlloc_nid_postfix(nullptr, 1); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    TestFreeTileInfo();
    return 0;
}
