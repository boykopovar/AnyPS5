#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "FastBitVector check failed at line %u\n", location.line());
        std::abort();
    }
}

using ClearRange = void (APS5_VABI *)(WTF::FastBitVector*, std::size_t, std::size_t);
static_assert(std::is_same_v<decltype(&_ZN3WTF13FastBitVector10clearRangeEmm), ClearRange>);
using Resize = void (APS5_VABI *)(WTF::FastBitVectorWordOwner*, std::size_t);
using Copy = void (APS5_VABI *)(WTF::FastBitVectorWordOwner*, const WTF::FastBitVectorWordOwner*);
static_assert(std::is_same_v<decltype(&_ZN3WTF22FastBitVectorWordOwner10resizeSlowEm), Resize>);
static_assert(std::is_same_v<decltype(&_ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_), Copy>);

struct GuardedOwner {
    std::uint64_t before = 0x123456789ABCDEF0;
    WTF::FastBitVectorWordOwner owner;
    std::uint64_t after = 0xFEDCBA9876543210;

    GuardedOwner(std::size_t bits) {
        owner.numBits = bits;
        const auto count = (bits + 31) / 32;
        if (count) {
            owner.words = static_cast<std::uint32_t*>(_ZN3WTF10fastMallocEm(count * sizeof(std::uint32_t)));
            for (std::size_t i = 0; i < count; ++i) owner.words[i] = 0xA5A50000u + static_cast<std::uint32_t>(i);
        }
    }

    ~GuardedOwner() {
        _ZN3WTF8fastFreeEPv(owner.words);
    }

    void CheckGuards() const {
        Require(before == 0x123456789ABCDEF0 && after == 0xFEDCBA9876543210);
    }
};

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

void TestClearRange() {
    ClearRange clear = &_ZN3WTF13FastBitVector10clearRangeEmm;
    std::mt19937 random(13117);
    std::size_t cases = 0;
    for (std::size_t bits = 0; bits <= 130; ++bits) {
        const auto words = (bits + 31) / 32;
        for (std::size_t begin = 0; begin <= bits; ++begin) {
            for (std::size_t end = begin; end <= bits; ++end) {
                std::vector<std::uint32_t> guarded(words + 2);
                for (auto& word : guarded) word = random();
                const auto before = guarded;
                WTF::FastBitVector vector{{guarded.data() + 1, bits}};
                const auto original = vector;
                clear(&vector, begin, end);
                Require(vector.storage.words == original.storage.words && vector.storage.numBits == bits);
                Require(guarded.front() == before.front() && guarded.back() == before.back());
                for (std::size_t bit = 0; bit < words * 32; ++bit) {
                    const auto mask = std::uint32_t{1} << (bit % 32);
                    const bool expected = bit >= begin && bit < end ? false : (before[bit / 32 + 1] & mask) != 0;
                    Require(((guarded[bit / 32 + 1] & mask) != 0) == expected);
                }
                ++cases;
            }
        }
    }
    WTF::FastBitVector empty;
    clear(&empty, 0, 0);
    RequireThrows([&] { clear(nullptr, 0, 0); });
    RequireThrows([&] { clear(&empty, 0, 1); });
    RequireThrows([&] { clear(&empty, 1, 0); });
    WTF::FastBitVector missing{{nullptr, 64}};
    RequireThrows([&] { clear(&missing, 0, 64); });
    std::array<std::uint32_t, 2> words{0x12345678, 0xABCDEF01};
    const auto before = words;
    WTF::FastBitVector guarded{{words.data(), 63}};
    RequireThrows([&] { clear(&guarded, 0, 64); });
    RequireThrows([&] { clear(&guarded, 32, 31); });
    WTF::FastBitVector overflow{{words.data(), std::numeric_limits<std::size_t>::max()}};
    RequireThrows([&] { clear(&overflow, 0, 1); });
    Require(words == before);
    std::printf("FastBitVector clearRange: %zu exhaustive interval cases passed\n", cases);
}

void TestResize() {
    Resize resize = &_ZN3WTF22FastBitVectorWordOwner10resizeSlowEm;
    std::size_t cases = 0;
    for (std::size_t bits = 0; bits <= 130; ++bits) {
        const auto oldCount = (bits + 31) / 32;
        for (std::size_t newBits = 0; newBits <= 260; ++newBits) {
            const auto newCount = (newBits + 31) / 32;
            if (newCount < oldCount) continue;
            GuardedOwner value(bits);
            const auto oldWords = value.owner.words;
            resize(&value.owner, newBits);
            value.CheckGuards();
            Require(value.owner.numBits == bits && value.owner.words != nullptr);
            Require(value.owner.words != oldWords);
            Require(_ZN3WTF14fastMallocSizeEPKv(value.owner.words) == newCount * sizeof(std::uint32_t));
            for (std::size_t i = 0; i < oldCount; ++i)
                Require(value.owner.words[i] == 0xA5A50000u + static_cast<std::uint32_t>(i));
            for (std::size_t i = oldCount; i < newCount; ++i) Require(value.owner.words[i] == 0);
            value.owner.numBits = newBits;
            ++cases;
        }
    }
    GuardedOwner value(65);
    const auto original = value.owner;
    const std::array<std::uint32_t, 3> contents{value.owner.words[0], value.owner.words[1], value.owner.words[2]};
    RequireThrows([&] { resize(nullptr, 0); });
    RequireThrows([&] { resize(&value.owner, 32); });
    RequireThrows([&] { resize(&value.owner, std::numeric_limits<std::size_t>::max()); });
    Require(value.owner.words == original.words && value.owner.numBits == original.numBits);
    Require(std::memcmp(value.owner.words, contents.data(), sizeof(contents)) == 0);
    value.CheckGuards();
    WTF::FastBitVectorWordOwner missing{nullptr, 1};
    RequireThrows([&] { resize(&missing, 64); });
    Require(missing.words == nullptr && missing.numBits == 1);
    std::printf("FastBitVector resizeSlow: %zu boundary and growth cases passed\n", cases);
}

void TestCopy() {
    Copy copy = &_ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_;
    std::size_t cases = 0;
    for (std::size_t bits = 0; bits <= 130; ++bits) {
        for (std::size_t oldBits = 0; oldBits <= 130; ++oldBits) {
            GuardedOwner source(bits);
            GuardedOwner target(oldBits);
            const auto sourcePointer = source.owner.words;
            copy(&target.owner, &source.owner);
            source.CheckGuards();
            target.CheckGuards();
            Require(target.owner.numBits == bits && target.owner.words != nullptr);
            Require(source.owner.words == sourcePointer && source.owner.numBits == bits);
            Require(target.owner.words != source.owner.words);
            const auto count = (bits + 31) / 32;
            Require(_ZN3WTF14fastMallocSizeEPKv(target.owner.words) == count * sizeof(std::uint32_t));
            for (std::size_t i = 0; i < count; ++i) Require(target.owner.words[i] == source.owner.words[i]);
            if (count) {
                target.owner.words[count - 1] ^= 1;
                Require(target.owner.words[count - 1] != source.owner.words[count - 1]);
            }
            ++cases;
        }
        GuardedOwner self(bits);
        copy(&self.owner, &self.owner);
        self.CheckGuards();
        Require(self.owner.numBits == bits);
        for (std::size_t i = 0; i < (bits + 31) / 32; ++i)
            Require(self.owner.words[i] == 0xA5A50000u + static_cast<std::uint32_t>(i));
    }
    GuardedOwner source(33);
    GuardedOwner target(65);
    const auto original = target.owner;
    RequireThrows([&] { copy(nullptr, &source.owner); });
    RequireThrows([&] { copy(&target.owner, nullptr); });
    WTF::FastBitVectorWordOwner missing{nullptr, 1};
    RequireThrows([&] { copy(&target.owner, &missing); });
    WTF::FastBitVectorWordOwner overflow{nullptr, std::numeric_limits<std::size_t>::max()};
    RequireThrows([&] { copy(&target.owner, &overflow); });
    Require(target.owner.words == original.words && target.owner.numBits == original.numBits);
    for (std::size_t i = 0; i < 3; ++i)
        Require(target.owner.words[i] == 0xA5A50000u + static_cast<std::uint32_t>(i));
    target.CheckGuards();
    std::printf("FastBitVector setEqualsSlow: %zu copy cases and 131 self-copies passed\n", cases);
}

}

int main() {
    TestClearRange();
    TestResize();
    TestCopy();
}
