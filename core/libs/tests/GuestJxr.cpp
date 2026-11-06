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
#include <limits>
#include <stdexcept>

namespace {

void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "GuestJxr.cpp:%d: requirement failed\n", line); std::abort(); } }
#define Require(value) Check((value), __LINE__)

void* lastFreed = nullptr;
int frees = 0;
std::array<void*, 32> freeLog{};
bool failAllocation = false;
bool fillMalloc = false;
int mallocCalls = 0;
int failMallocAt = -1;
std::array<void*, 32> mallocLog{};
std::array<std::size_t, 32> mallocSizeLog{};
std::size_t lastCount = 0;
std::size_t lastSize = 0;
void* lastAllocated = nullptr;
int callocCalls = 0;

void* APS5_VABI Allocate(std::size_t size) {
    ++mallocCalls;
    void* pointer = mallocCalls == failMallocAt ? nullptr : std::malloc(size);
    if (pointer && fillMalloc) std::memset(pointer, 0xa5, size);
    if (mallocCalls <= static_cast<int>(mallocLog.size())) {
        mallocLog[static_cast<std::size_t>(mallocCalls - 1)] = pointer;
        mallocSizeLog[static_cast<std::size_t>(mallocCalls - 1)] = size;
    }
    return pointer;
}
void APS5_VABI Free(void* pointer) {
    if (pointer != nullptr) {
        lastFreed = pointer;
        if (frees < static_cast<int>(freeLog.size())) freeLog[static_cast<std::size_t>(frees)] = pointer;
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

void TestInitPostProc() {
    fillMalloc = true;
    JxrGuestPostProcInfo sentinel{};
    JxrGuestPostProcInfo* slots[4][2];
    for (auto& channel : slots)
        for (auto& slot : channel) slot = &sentinel;

    constexpr std::size_t width = 2;
    const auto firstAllocation = mallocCalls;
    Require(initPostProc_nid_postfix(slots, width, 2) == 0);
    Require(mallocCalls == firstAllocation + 4);
    for (std::size_t channel = 0; channel < 2; ++channel) {
        for (std::size_t slot = 0; slot < 2; ++slot) {
            const auto logIndex = static_cast<std::size_t>(firstAllocation + channel * 2 + slot);
            auto* const base = static_cast<JxrGuestPostProcInfo*>(mallocLog[logIndex]);
            Require(base != nullptr && slots[channel][slot] == base + 1);
            Require(mallocSizeLog[logIndex] == (width + 2) * sizeof(JxrGuestPostProcInfo));
            const auto checkBoundary = [](const JxrGuestPostProcInfo* value) {
                Require(value->mbTexture == 3);
                for (const auto& row : value->blockTexture)
                    for (const auto texture : row) Require(texture == 3);
            };
            checkBoundary(slots[channel][slot] - 1);
            checkBoundary(slots[channel][slot] + width);
            for (std::size_t i = 0; i < width; ++i) {
                const auto* bytes = reinterpret_cast<const unsigned char*>(slots[channel][slot] + i);
                for (std::size_t j = 0; j < sizeof(JxrGuestPostProcInfo); ++j) Require(bytes[j] == 0xa5);
            }
            Require(std::memcmp(base, slots[channel][slot] + width, sizeof(*base)) == 0);
        }
    }
    for (std::size_t channel = 2; channel < 4; ++channel)
        for (const auto* slot : slots[channel]) Require(slot == &sentinel);
    for (std::size_t channel = 0; channel < 2; ++channel)
        for (std::size_t slot = 0; slot < 2; ++slot) free_nid_postfix(slots[channel][slot] - 1);

    const auto allocationsBeforeZeroChannels = mallocCalls;
    Require(initPostProc_nid_postfix(nullptr, std::numeric_limits<std::size_t>::max(), 0) == 0);
    Require(mallocCalls == allocationsBeforeZeroChannels);

    JxrGuestPostProcInfo* overflowSlots[1][2] = {{&sentinel, &sentinel}};
    const auto allocationsBeforeOverflow = mallocCalls;
    constexpr auto overflowWidth = std::size_t{745} * 65536 - 2;
    Require(initPostProc_nid_postfix(overflowSlots, overflowWidth, 1) == -1);
    Require(mallocCalls == allocationsBeforeOverflow);
    Require(overflowSlots[0][0] == &sentinel && overflowSlots[0][1] == &sentinel);

    JxrGuestPostProcInfo* partialSlots[3][2];
    for (auto& channel : partialSlots)
        for (auto& slot : channel) slot = &sentinel;
    const auto firstPartialAllocation = mallocCalls;
    failMallocAt = mallocCalls + 3;
    Require(initPostProc_nid_postfix(partialSlots, 1, 2) == -1);
    failMallocAt = -1;
    Require(mallocCalls == firstPartialAllocation + 3);
    Require(partialSlots[0][0] == static_cast<JxrGuestPostProcInfo*>(mallocLog[static_cast<std::size_t>(firstPartialAllocation)]) + 1);
    Require(partialSlots[0][1] == static_cast<JxrGuestPostProcInfo*>(mallocLog[static_cast<std::size_t>(firstPartialAllocation + 1)]) + 1);
    Require(partialSlots[1][0] == nullptr && partialSlots[1][1] == &sentinel);
    Require(partialSlots[2][0] == &sentinel && partialSlots[2][1] == &sentinel);
    free_nid_postfix(partialSlots[0][0] - 1);
    free_nid_postfix(partialSlots[0][1] - 1);
    fillMalloc = false;
}

}

void TestTermPostProc() {
    JxrGuestPostProcInfo sentinel{};
    JxrGuestPostProcInfo* slots[3][2];
    for (auto& channel : slots)
        for (auto& slot : channel) slot = &sentinel;

    std::array<JxrGuestPostProcInfo*, 3> base{};
    const auto firstAllocation = mallocCalls;
    for (auto& pointer : base) {
        pointer = static_cast<JxrGuestPostProcInfo*>(malloc_nid_postfix(sizeof(JxrGuestPostProcInfo)));
        Require(pointer != nullptr);
    }
    Require(mallocCalls == firstAllocation + 3);
    for (std::size_t i = 0; i < base.size(); ++i) {
        const auto index = static_cast<std::size_t>(firstAllocation) + i;
        Require(mallocLog[index] == base[i]);
        Require(mallocSizeLog[index] == 88);
    }

    slots[0][0] = base[0] + 1;
    slots[0][1] = nullptr;
    slots[1][0] = base[1] + 1;
    slots[1][1] = base[2] + 1;
    const auto firstFree = frees;
    termPostProc_nid_postfix(slots, 2);
    Require(frees == firstFree + 3);
    for (std::size_t i = 0; i < base.size(); ++i)
        Require(freeLog[static_cast<std::size_t>(firstFree) + i] == base[i]);
    Require(slots[0][0] == base[0] + 1 && slots[0][1] == nullptr);
    Require(slots[1][0] == base[1] + 1 && slots[1][1] == base[2] + 1);
    for (std::size_t slot = 0; slot < 2; ++slot) Require(slots[2][slot] == &sentinel);

    const auto freesBeforeZeroChannels = frees;
    termPostProc_nid_postfix(nullptr, 0);
    Require(frees == freesBeforeZeroChannels);

    bool threw = false;
    try { termPostProc_nid_postfix(nullptr, 1); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
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
    TestTermPostProc();
    TestInitPostProc();
    return 0;
}
