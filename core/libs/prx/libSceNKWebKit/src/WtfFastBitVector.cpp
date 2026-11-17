#include "prx/libSceNKWebKit/include/WtfFastBitVector.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

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

WTF::FastBitVectorWordOwner LoadOwner(const WTF::FastBitVectorWordOwner* owner, const char* func) {
    if (!owner) throw std::invalid_argument("FastBitVector: null word owner");
    WTF::FastBitVectorWordOwner copy;
    if (!GuestCopyIn_nid_no_patch(&copy, owner, sizeof(copy))) NotImplementedAt_nid_no_patch(func, __FILE__, __LINE__);
    return copy;
}

std::size_t CheckedWordCount(const WTF::FastBitVectorWordOwner& owner) {
    const auto count = WordCount(owner.numBits);
    if (count && !owner.words) throw std::invalid_argument("FastBitVector: null nonempty word storage");
    return count;
}

}

extern "C" void APS5_VABI _ZN3WTF13FastBitVector10clearRangeEmm(WTF::FastBitVector* vector,
    std::size_t begin, std::size_t end) {
    if (!vector) throw std::invalid_argument("FastBitVector::clearRange: null vector");
    WTF::FastBitVectorWordOwner storage;
    if (!GuestCopyIn_nid_no_patch(&storage, &vector->storage, sizeof(storage))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
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
    const auto current = LoadOwner(owner, __func__);
    const auto oldCount = CheckedWordCount(current);
    const auto newCount = WordCount(numBits);
    if (newCount < oldCount) throw std::invalid_argument("FastBitVector::resizeSlow: shrinking word storage");
    auto* words = static_cast<std::uint32_t*>(_ZN3WTF10fastMallocEm(newCount * sizeof(std::uint32_t)));
    if (oldCount) std::memcpy(words, current.words, oldCount * sizeof(std::uint32_t));
    std::fill_n(words + oldCount, newCount - oldCount, std::uint32_t{0});
    if (!GuestCopyOut_nid_no_patch(&owner->words, &words, sizeof(words))) {
        _ZN3WTF8fastFreeEPv(words);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    _ZN3WTF8fastFreeEPv(current.words);
}

extern "C" void APS5_VABI _ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_(WTF::FastBitVectorWordOwner* owner,
    const WTF::FastBitVectorWordOwner* other) {
    const auto current = LoadOwner(owner, __func__);
    CheckedWordCount(current);
    const auto source = LoadOwner(other, __func__);
    const auto count = CheckedWordCount(source);
    auto* words = static_cast<std::uint32_t*>(_ZN3WTF10fastMallocEm(count * sizeof(std::uint32_t)));
    if (count) std::memcpy(words, source.words, count * sizeof(std::uint32_t));
    const WTF::FastBitVectorWordOwner result{words, source.numBits};
    if (!GuestCopyOut_nid_no_patch(owner, &result, sizeof(result))) {
        _ZN3WTF8fastFreeEPv(words);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    _ZN3WTF8fastFreeEPv(current.words);
}
