#include "prx/libSceNKWebKit/include/WtfFastBitVector.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

std::size_t WordCount(std::size_t bits) {
    if (bits > std::numeric_limits<std::size_t>::max() - 31)
        throw std::length_error("FastBitVector: bit count overflows word rounding");
    return (bits + 31) / 32;
}

std::size_t CheckedWordCount(const WTF::FastBitVectorWordOwner* owner) {
    if (!owner) throw std::invalid_argument("FastBitVector: null word owner");
    const auto count = WordCount(owner->numBits);
    if (count && !owner->words) throw std::invalid_argument("FastBitVector: null nonempty word storage");
    return count;
}

}

extern "C" void APS5_VABI _ZN3WTF13FastBitVector10clearRangeEmm(WTF::FastBitVector* vector,
    std::size_t begin, std::size_t end) {
    if (!vector) throw std::invalid_argument("FastBitVector::clearRange: null vector");
    auto& storage = vector->storage;
    if (begin > end || end > storage.numBits)
        throw std::out_of_range("FastBitVector::clearRange: invalid bit range");
    if (begin == end) return;
    WordCount(storage.numBits);
    if (!storage.words) throw std::invalid_argument("FastBitVector::clearRange: null words");
    const auto firstWord = begin / 32;
    const auto lastWord = (end - 1) / 32;
    const auto firstMask = std::numeric_limits<std::uint32_t>::max() << (begin % 32);
    const auto lastBits = end % 32;
    const auto lastMask = lastBits ? (std::uint32_t{1} << lastBits) - 1 : std::numeric_limits<std::uint32_t>::max();
    if (firstWord == lastWord) {
        storage.words[firstWord] &= ~(firstMask & lastMask);
        return;
    }
    storage.words[firstWord] &= ~firstMask;
    std::fill_n(storage.words + firstWord + 1, lastWord - firstWord - 1, std::uint32_t{0});
    storage.words[lastWord] &= ~lastMask;
}

extern "C" void APS5_VABI _ZN3WTF22FastBitVectorWordOwner10resizeSlowEm(WTF::FastBitVectorWordOwner* owner,
    std::size_t numBits) {
    const auto oldCount = CheckedWordCount(owner);
    const auto newCount = WordCount(numBits);
    if (newCount < oldCount) throw std::invalid_argument("FastBitVector::resizeSlow: shrinking word storage");
    auto* words = static_cast<std::uint32_t*>(_ZN3WTF10fastMallocEm(newCount * sizeof(std::uint32_t)));
    if (oldCount) std::memcpy(words, owner->words, oldCount * sizeof(std::uint32_t));
    std::fill_n(words + oldCount, newCount - oldCount, std::uint32_t{0});
    _ZN3WTF8fastFreeEPv(owner->words);
    owner->words = words;
}

extern "C" void APS5_VABI _ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_(WTF::FastBitVectorWordOwner* owner,
    const WTF::FastBitVectorWordOwner* other) {
    CheckedWordCount(owner);
    const auto count = CheckedWordCount(other);
    auto* words = static_cast<std::uint32_t*>(_ZN3WTF10fastMallocEm(count * sizeof(std::uint32_t)));
    if (count) std::memcpy(words, other->words, count * sizeof(std::uint32_t));
    _ZN3WTF8fastFreeEPv(owner->words);
    owner->words = words;
    owner->numBits = other->numBits;
}
