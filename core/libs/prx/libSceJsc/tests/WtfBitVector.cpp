#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

using Jsc::WtfAbi::BitVector;

namespace {

constexpr std::uintptr_t InlineFlag = std::uintptr_t{1} << 63;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF BitVector check failed at line %d\n", line);
        std::abort();
    }
}

struct alignas(16) Storage {
    std::size_t numBits;
    std::array<std::uintptr_t, 4> words;
};

BitVector OutOfLine(Storage& storage) {
    return {reinterpret_cast<std::uintptr_t>(&storage) >> 1};
}

BitVector Inline(std::uintptr_t bits) {
    return {bits | InlineFlag};
}

void Filter(BitVector& self, const BitVector& other) {
    _ZN3WTF9BitVector10filterSlowERKS0_(&self, &other);
}

template<typename TException, typename TFunction> bool Throws(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

bool failNextAllocation = false;
std::size_t allocationCount = 0;
std::size_t releaseCount = 0;
void* lastReleased = nullptr;

void* APS5_VABI HeapAllocate(std::size_t bytes) {
    ++allocationCount;
    if (failNextAllocation) {
        failNextAllocation = false;
        return nullptr;
    }
    return std::malloc(bytes);
}

void APS5_VABI HeapFree(void* pointer) {
    ++releaseCount;
    lastReleased = pointer;
    std::free(pointer);
}

void* APS5_VABI HeapCalloc(std::size_t count, std::size_t bytes) {
    return std::calloc(count, bytes);
}

void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) {
    return std::realloc(pointer, bytes);
}

void* APS5_VABI HeapAlign(std::size_t, std::size_t bytes) {
    return std::malloc(bytes);
}

void* APS5_VABI HeapRealign(void* pointer, std::size_t bytes, std::size_t) {
    return std::realloc(pointer, bytes);
}

int APS5_VABI HeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer != nullptr ? 0 : 12;
}

void InitializeHeap() {
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&HeapAllocate);
    api[1] = reinterpret_cast<void*>(&HeapFree);
    api[2] = reinterpret_cast<void*>(&HeapCalloc);
    api[3] = reinterpret_cast<void*>(&HeapReallocate);
    api[4] = reinterpret_cast<void*>(&HeapAlign);
    api[5] = reinterpret_cast<void*>(&HeapRealign);
    api[6] = reinterpret_cast<void*>(&HeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());
}

void Resize(BitVector& self, std::size_t numBits) {
    _ZN3WTF9BitVector15resizeOutOfLineEm(&self, numBits);
}

std::size_t* Bits(const BitVector& vector) {
    return reinterpret_cast<std::size_t*>(vector.bitsOrPointer << 1);
}

std::uintptr_t* Words(const BitVector& vector) {
    return reinterpret_cast<std::uintptr_t*>(Bits(vector) + 1);
}

void TestResizeOutOfLine() {
    auto vector = Inline(0x8000000000000F0FULL);
    Resize(vector, 100);
    Check((vector.bitsOrPointer & InlineFlag) == 0, __LINE__);
    Check(Bits(vector)[0] == 128, __LINE__);
    Check(Words(vector)[0] == 0x0000000000000F0FULL && Words(vector)[1] == 0, __LINE__);

    Words(vector)[0] = 0x1111111111111111ULL;
    Words(vector)[1] = 0x2222222222222222ULL;
    std::size_t* previous = Bits(vector);
    Resize(vector, 300);
    Check(Bits(vector) != previous, __LINE__);
    Check(Bits(vector)[0] == 320, __LINE__);
    Check(Words(vector)[0] == 0x1111111111111111ULL && Words(vector)[1] == 0x2222222222222222ULL, __LINE__);
    for (int i = 2; i < 5; ++i) Check(Words(vector)[i] == 0, __LINE__);

    Words(vector)[2] = 0x3333333333333333ULL;
    Resize(vector, 65);
    Check(Bits(vector)[0] == 128, __LINE__);
    Check(Words(vector)[0] == 0x1111111111111111ULL && Words(vector)[1] == 0x2222222222222222ULL, __LINE__);

    Resize(vector, 128);
    Check(Bits(vector)[0] == 128, __LINE__);
    Check(Words(vector)[0] == 0x1111111111111111ULL && Words(vector)[1] == 0x2222222222222222ULL, __LINE__);
    free_nid_postfix(Bits(vector));

    auto exact = Inline(0x7FFFFFFFFFFFFFFFULL);
    Resize(exact, 64);
    Check(Bits(exact)[0] == 64 && Words(exact)[0] == 0x7FFFFFFFFFFFFFFFULL, __LINE__);
    free_nid_postfix(Bits(exact));

    auto small = Inline(0x5);
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF9BitVector15resizeOutOfLineEm(nullptr, 100); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { Resize(small, 63); }), __LINE__);
    Check(small.bitsOrPointer == (0x5 | InlineFlag), __LINE__);
    for (const std::uintptr_t invalid : {std::uintptr_t{0}, std::uintptr_t{1}}) {
        BitVector copy{invalid};
        Check(Throws<std::invalid_argument>([&] { Resize(copy, 100); }), __LINE__);
        Check(copy.bitsOrPointer == invalid, __LINE__);
    }
    Check(Throws<std::length_error>([&] { Resize(small, SIZE_MAX); }), __LINE__);
    Check(small.bitsOrPointer == (0x5 | InlineFlag), __LINE__);
}

void CheckBytes(const unsigned char* data, std::size_t size, unsigned char expected, int line) {
    for (std::size_t i = 0; i < size; ++i) Check(data[i] == expected, line);
}

void TestResizeOutOfLineAllocationRollback() {
    constexpr std::size_t LiveSize = 64;
    auto* live = static_cast<unsigned char*>(malloc_nid_postfix(LiveSize));
    Check(live != nullptr, __LINE__);
    std::memset(live, 0xA5, LiveSize);

    auto vector = Inline(0x5A5A5A5A5A5A5A5AULL);
    const std::uintptr_t originalInlineBits = vector.bitsOrPointer;
    std::size_t allocationsBefore = allocationCount;
    std::size_t releasesBefore = releaseCount;
    failNextAllocation = true;
    Check(Throws<std::bad_alloc>([&] { Resize(vector, 200); }), __LINE__);
    Check(!failNextAllocation, __LINE__);
    Check(allocationCount == allocationsBefore + 1, __LINE__);
    Check(releaseCount == releasesBefore, __LINE__);
    Check(vector.bitsOrPointer == originalInlineBits, __LINE__);
    CheckBytes(live, LiveSize, 0xA5, __LINE__);

    Resize(vector, 200);
    Check((vector.bitsOrPointer & InlineFlag) == 0, __LINE__);
    Check(Bits(vector)[0] == 256, __LINE__);
    Check(Words(vector)[0] == 0x5A5A5A5A5A5A5A5AULL, __LINE__);
    for (int i = 1; i < 4; ++i) Check(Words(vector)[i] == 0, __LINE__);
    Check(releaseCount == releasesBefore, __LINE__);

    Words(vector)[0] = 0x1111111111111111ULL;
    Words(vector)[1] = 0x2222222222222222ULL;
    Words(vector)[2] = 0x3333333333333333ULL;
    Words(vector)[3] = 0x4444444444444444ULL;
    std::size_t* const oldBlock = Bits(vector);
    const std::uintptr_t originalHeapBits = vector.bitsOrPointer;
    allocationsBefore = allocationCount;
    releasesBefore = releaseCount;
    failNextAllocation = true;
    Check(Throws<std::bad_alloc>([&] { Resize(vector, 300); }), __LINE__);
    Check(!failNextAllocation, __LINE__);
    Check(allocationCount == allocationsBefore + 1, __LINE__);
    Check(releaseCount == releasesBefore, __LINE__);
    Check(vector.bitsOrPointer == originalHeapBits, __LINE__);
    Check(Bits(vector) == oldBlock, __LINE__);
    Check(Bits(vector)[0] == 256, __LINE__);
    Check(Words(vector)[0] == 0x1111111111111111ULL && Words(vector)[1] == 0x2222222222222222ULL, __LINE__);
    Check(Words(vector)[2] == 0x3333333333333333ULL && Words(vector)[3] == 0x4444444444444444ULL, __LINE__);
    CheckBytes(live, LiveSize, 0xA5, __LINE__);

    lastReleased = nullptr;
    Resize(vector, 300);
    Check(Bits(vector) != oldBlock, __LINE__);
    Check(releaseCount == releasesBefore + 1, __LINE__);
    Check(lastReleased == oldBlock, __LINE__);
    Check(Bits(vector)[0] == 320, __LINE__);
    Check(Words(vector)[0] == 0x1111111111111111ULL && Words(vector)[1] == 0x2222222222222222ULL, __LINE__);
    Check(Words(vector)[2] == 0x3333333333333333ULL && Words(vector)[3] == 0x4444444444444444ULL, __LINE__);
    Check(Words(vector)[4] == 0, __LINE__);
    CheckBytes(live, LiveSize, 0xA5, __LINE__);

    free_nid_postfix(Bits(vector));
    free_nid_postfix(live);
}

}

int main() {
    {
        Storage a{130, {0xF0F0F0F0F0F0F0F0ULL, 0xFFFFFFFFFFFFFFFFULL, 0x3ULL, 0xA5A5A5A5A5A5A5A5ULL}};
        auto self = OutOfLine(a);
        const auto other = Inline(0x00FF00FF00FF00FFULL);
        Filter(self, other);
        Check(self.bitsOrPointer == OutOfLine(a).bitsOrPointer, __LINE__);
        Check(a.numBits == 130, __LINE__);
        Check(a.words[0] == 0x00F000F000F000F0ULL, __LINE__);
        Check(a.words[1] == 0xFFFFFFFFFFFFFFFFULL && a.words[2] == 0x3ULL && a.words[3] == 0xA5A5A5A5A5A5A5A5ULL, __LINE__);
        Check(other.bitsOrPointer == (0x00FF00FF00FF00FFULL | InlineFlag), __LINE__);
    }
    {
        Storage a{64, {0xFFFFFFFFFFFFFFFFULL, 0, 0, 0}};
        auto self = OutOfLine(a);
        Filter(self, Inline(0x7FFFFFFFFFFFFFFFULL));
        Check(a.words[0] == 0x7FFFFFFFFFFFFFFFULL, __LINE__);
    }
    for (const std::uintptr_t first : {0x8000000000000F0FULL, 0x0000000000000F0FULL}) {
        Storage b{65, {first, 0xFFFFFFFFFFFFFFFFULL, 0xA5, 0xA5}};
        auto self = Inline(0x00000000000033FFULL);
        const auto other = OutOfLine(b);
        Filter(self, other);
        Check(self.bitsOrPointer == (0x000000000000030FULL | InlineFlag), __LINE__);
        Check(b.numBits == 65 && b.words[0] == first && b.words[1] == 0xFFFFFFFFFFFFFFFFULL, __LINE__);
    }
    {
        Storage a{130, {0xFF00FF00FF00FF00ULL, 0x0F0F0F0F0F0F0F0FULL, 0x3ULL, 0xA5A5A5A5A5A5A5A5ULL}};
        Storage b{65, {0xF0F0F0F0F0F0F0F0ULL, 0x00000000FFFFFFFFULL, 0x1ULL, 0x1ULL}};
        auto self = OutOfLine(a);
        Filter(self, OutOfLine(b));
        Check(a.numBits == 130, __LINE__);
        Check(a.words[0] == 0xF000F000F000F000ULL, __LINE__);
        Check(a.words[1] == 0x000000000F0F0F0FULL, __LINE__);
        Check(a.words[2] == 0, __LINE__);
        Check(a.words[3] == 0xA5A5A5A5A5A5A5A5ULL, __LINE__);
        Check(b.words[0] == 0xF0F0F0F0F0F0F0F0ULL && b.words[1] == 0x00000000FFFFFFFFULL && b.words[2] == 0x1ULL, __LINE__);
    }
    {
        Storage a{128, {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xA5, 0xA5}};
        Storage b{192, {0x1234ULL, 0x5678ULL, 0x0ULL, 0xA5}};
        auto self = OutOfLine(a);
        Filter(self, OutOfLine(b));
        Check(a.numBits == 128 && a.words[0] == 0x1234ULL && a.words[1] == 0x5678ULL && a.words[2] == 0xA5, __LINE__);
    }
    {
        Storage a{1, {0x3ULL, 0xA5, 0xA5, 0xA5}};
        Storage b{1, {0x2ULL, 0xA5, 0xA5, 0xA5}};
        auto self = OutOfLine(a);
        Filter(self, OutOfLine(b));
        Check(a.words[0] == 0x2ULL && a.words[1] == 0xA5, __LINE__);
    }
    {
        Storage a{64, {0xFFULL, 0, 0, 0}};
        auto self = OutOfLine(a);
        auto inlineSelf = Inline(0xFF);
        const BitVector empty{0};
        const BitVector deleted{1};
        Check(Throws<std::invalid_argument>([&] { _ZN3WTF9BitVector10filterSlowERKS0_(nullptr, &self); }), __LINE__);
        Check(Throws<std::invalid_argument>([&] { _ZN3WTF9BitVector10filterSlowERKS0_(&self, nullptr); }), __LINE__);
        Check(Throws<std::invalid_argument>([&] { Filter(inlineSelf, Inline(0xF)); }), __LINE__);
        Check(inlineSelf.bitsOrPointer == (0xFFULL | InlineFlag), __LINE__);
        for (const auto& invalid : {empty, deleted}) {
            auto copy = invalid;
            Check(Throws<std::invalid_argument>([&] { Filter(copy, Inline(0xF)); }), __LINE__);
            Check(copy.bitsOrPointer == invalid.bitsOrPointer, __LINE__);
            Check(Throws<std::invalid_argument>([&] { Filter(inlineSelf, invalid); }), __LINE__);
            Check(inlineSelf.bitsOrPointer == (0xFFULL | InlineFlag), __LINE__);
            Check(Throws<std::invalid_argument>([&] { Filter(self, invalid); }), __LINE__);
            Check(a.words[0] == 0xFFULL, __LINE__);
        }
    }
    InitializeHeap();
    TestResizeOutOfLine();
    TestResizeOutOfLineAllocationRollback();
    return 0;
}
