#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "Testing.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using Jsc::WtfAbi::BitVector;

namespace {

constexpr std::uintptr_t InlineFlag = std::uintptr_t{1} << 63;

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

template<typename Function> bool ThrowsInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
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
    Require((vector.bitsOrPointer & InlineFlag) == 0);
    Require(Bits(vector)[0] == 128);
    Require(Words(vector)[0] == 0x0000000000000F0FULL && Words(vector)[1] == 0);

    Words(vector)[0] = 0x1111111111111111ULL;
    Words(vector)[1] = 0x2222222222222222ULL;
    std::size_t* previous = Bits(vector);
    Resize(vector, 300);
    Require(Bits(vector) != previous);
    Require(Bits(vector)[0] == 320);
    Require(Words(vector)[0] == 0x1111111111111111ULL && Words(vector)[1] == 0x2222222222222222ULL);
    for (int i = 2; i < 5; ++i) Require(Words(vector)[i] == 0);

    Words(vector)[2] = 0x3333333333333333ULL;
    Resize(vector, 65);
    Require(Bits(vector)[0] == 128);
    Require(Words(vector)[0] == 0x1111111111111111ULL && Words(vector)[1] == 0x2222222222222222ULL);

    Resize(vector, 128);
    Require(Bits(vector)[0] == 128);
    Require(Words(vector)[0] == 0x1111111111111111ULL && Words(vector)[1] == 0x2222222222222222ULL);
    free_nid_postfix(Bits(vector));

    auto exact = Inline(0x7FFFFFFFFFFFFFFFULL);
    Resize(exact, 64);
    Require(Bits(exact)[0] == 64 && Words(exact)[0] == 0x7FFFFFFFFFFFFFFFULL);
    free_nid_postfix(Bits(exact));

    auto small = Inline(0x5);
    Require(ThrowsInvalidArgument([&] { _ZN3WTF9BitVector15resizeOutOfLineEm(nullptr, 100); }));
    Require(ThrowsInvalidArgument([&] { Resize(small, 63); }));
    Require(small.bitsOrPointer == (0x5 | InlineFlag));
    for (const std::uintptr_t invalid : {std::uintptr_t{0}, std::uintptr_t{1}}) {
        BitVector copy{invalid};
        Require(ThrowsInvalidArgument([&] { Resize(copy, 100); }));
        Require(copy.bitsOrPointer == invalid);
    }
    bool overflowed = false;
    try {
        Resize(small, SIZE_MAX);
    } catch (const std::length_error&) {
        overflowed = true;
    }
    Require(overflowed && small.bitsOrPointer == (0x5 | InlineFlag));
}

}

int main() {
    {
        Storage a{130, {0xF0F0F0F0F0F0F0F0ULL, 0xFFFFFFFFFFFFFFFFULL, 0x3ULL, 0xA5A5A5A5A5A5A5A5ULL}};
        auto self = OutOfLine(a);
        const auto other = Inline(0x00FF00FF00FF00FFULL);
        Filter(self, other);
        Require(self.bitsOrPointer == OutOfLine(a).bitsOrPointer);
        Require(a.numBits == 130);
        Require(a.words[0] == 0x00F000F000F000F0ULL);
        Require(a.words[1] == 0xFFFFFFFFFFFFFFFFULL && a.words[2] == 0x3ULL && a.words[3] == 0xA5A5A5A5A5A5A5A5ULL);
        Require(other.bitsOrPointer == (0x00FF00FF00FF00FFULL | InlineFlag));
    }
    {
        Storage a{64, {0xFFFFFFFFFFFFFFFFULL, 0, 0, 0}};
        auto self = OutOfLine(a);
        Filter(self, Inline(0x7FFFFFFFFFFFFFFFULL));
        Require(a.words[0] == 0x7FFFFFFFFFFFFFFFULL);
    }
    for (const std::uintptr_t first : {0x8000000000000F0FULL, 0x0000000000000F0FULL}) {
        Storage b{65, {first, 0xFFFFFFFFFFFFFFFFULL, 0xA5, 0xA5}};
        auto self = Inline(0x00000000000033FFULL);
        const auto other = OutOfLine(b);
        Filter(self, other);
        Require(self.bitsOrPointer == (0x000000000000030FULL | InlineFlag));
        Require(b.numBits == 65 && b.words[0] == first && b.words[1] == 0xFFFFFFFFFFFFFFFFULL);
    }
    {
        Storage a{130, {0xFF00FF00FF00FF00ULL, 0x0F0F0F0F0F0F0F0FULL, 0x3ULL, 0xA5A5A5A5A5A5A5A5ULL}};
        Storage b{65, {0xF0F0F0F0F0F0F0F0ULL, 0x00000000FFFFFFFFULL, 0x1ULL, 0x1ULL}};
        auto self = OutOfLine(a);
        Filter(self, OutOfLine(b));
        Require(a.numBits == 130);
        Require(a.words[0] == 0xF000F000F000F000ULL);
        Require(a.words[1] == 0x000000000F0F0F0FULL);
        Require(a.words[2] == 0);
        Require(a.words[3] == 0xA5A5A5A5A5A5A5A5ULL);
        Require(b.words[0] == 0xF0F0F0F0F0F0F0F0ULL && b.words[1] == 0x00000000FFFFFFFFULL && b.words[2] == 0x1ULL);
    }
    {
        Storage a{128, {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xA5, 0xA5}};
        Storage b{192, {0x1234ULL, 0x5678ULL, 0x0ULL, 0xA5}};
        auto self = OutOfLine(a);
        Filter(self, OutOfLine(b));
        Require(a.numBits == 128 && a.words[0] == 0x1234ULL && a.words[1] == 0x5678ULL && a.words[2] == 0xA5);
    }
    {
        Storage a{1, {0x3ULL, 0xA5, 0xA5, 0xA5}};
        Storage b{1, {0x2ULL, 0xA5, 0xA5, 0xA5}};
        auto self = OutOfLine(a);
        Filter(self, OutOfLine(b));
        Require(a.words[0] == 0x2ULL && a.words[1] == 0xA5);
    }
    {
        Storage a{64, {0xFFULL, 0, 0, 0}};
        auto self = OutOfLine(a);
        auto inlineSelf = Inline(0xFF);
        const BitVector empty{0};
        const BitVector deleted{1};
        Require(ThrowsInvalidArgument([&] { _ZN3WTF9BitVector10filterSlowERKS0_(nullptr, &self); }));
        Require(ThrowsInvalidArgument([&] { _ZN3WTF9BitVector10filterSlowERKS0_(&self, nullptr); }));
        Require(ThrowsInvalidArgument([&] { Filter(inlineSelf, Inline(0xF)); }));
        Require(inlineSelf.bitsOrPointer == (0xFFULL | InlineFlag));
        for (const auto& invalid : {empty, deleted}) {
            auto copy = invalid;
            Require(ThrowsInvalidArgument([&] { Filter(copy, Inline(0xF)); }));
            Require(copy.bitsOrPointer == invalid.bitsOrPointer);
            Require(ThrowsInvalidArgument([&] { Filter(inlineSelf, invalid); }));
            Require(inlineSelf.bitsOrPointer == (0xFFULL | InlineFlag));
            Require(ThrowsInvalidArgument([&] { Filter(self, invalid); }));
            Require(a.words[0] == 0xFFULL);
        }
    }
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    TestResizeOutOfLine();
    return 0;
}
