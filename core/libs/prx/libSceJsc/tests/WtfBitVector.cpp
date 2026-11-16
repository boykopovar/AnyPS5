#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

template<typename Function> bool ThrowsInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
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
        Check(ThrowsInvalidArgument([&] { _ZN3WTF9BitVector10filterSlowERKS0_(nullptr, &self); }), __LINE__);
        Check(ThrowsInvalidArgument([&] { _ZN3WTF9BitVector10filterSlowERKS0_(&self, nullptr); }), __LINE__);
        Check(ThrowsInvalidArgument([&] { Filter(inlineSelf, Inline(0xF)); }), __LINE__);
        Check(inlineSelf.bitsOrPointer == (0xFFULL | InlineFlag), __LINE__);
        for (const auto& invalid : {empty, deleted}) {
            auto copy = invalid;
            Check(ThrowsInvalidArgument([&] { Filter(copy, Inline(0xF)); }), __LINE__);
            Check(copy.bitsOrPointer == invalid.bitsOrPointer, __LINE__);
            Check(ThrowsInvalidArgument([&] { Filter(inlineSelf, invalid); }), __LINE__);
            Check(inlineSelf.bitsOrPointer == (0xFFULL | InlineFlag), __LINE__);
            Check(ThrowsInvalidArgument([&] { Filter(self, invalid); }), __LINE__);
            Check(a.words[0] == 0xFFULL, __LINE__);
        }
    }
    return 0;
}
