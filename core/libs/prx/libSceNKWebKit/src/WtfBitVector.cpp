#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfBitVector.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

using WTF::BitVector;
using OutOfLineBits = WTF::BitVector::OutOfLineBits;

constexpr std::size_t BitsInPointer = sizeof(void*) * 8;
constexpr std::size_t MaxInlineBits = BitsInPointer - 1;
constexpr std::uintptr_t InlineFlag = std::uintptr_t{1} << MaxInlineBits;

const BitVector& Checked(const BitVector* vector) {
    if (!vector) throw std::invalid_argument("null WTF::BitVector");
    return *vector;
}

BitVector& Checked(BitVector* vector) {
    if (!vector) throw std::invalid_argument("null WTF::BitVector");
    return *vector;
}

bool IsInline(const BitVector& vector) {
    return vector.bitsOrPointer >> MaxInlineBits;
}

bool IsEmptyOrDeletedValue(const BitVector& vector) {
    return vector.bitsOrPointer <= 1;
}

std::uintptr_t MakeInlineBits(std::uintptr_t bits) {
    return bits | InlineFlag;
}

std::uintptr_t CleanseInlineBits(std::uintptr_t bits) {
    return bits & ~InlineFlag;
}

OutOfLineBits* OutOfLine(const BitVector& vector) {
    return reinterpret_cast<OutOfLineBits*>(vector.bitsOrPointer << 1);
}

OutOfLineBits* RequireOutOfLine(const BitVector* vector) {
    const BitVector& checked = Checked(vector);
    if (IsInline(checked) || IsEmptyOrDeletedValue(checked))
        throw std::invalid_argument("WTF::BitVector slow path requires out-of-line bits");
    return OutOfLine(checked);
}

std::uintptr_t PointerBits(OutOfLineBits* bits) {
    return reinterpret_cast<std::uintptr_t>(bits) >> 1;
}

std::uintptr_t* Words(OutOfLineBits* bits) {
    return reinterpret_cast<std::uintptr_t*>(bits + 1);
}

std::size_t NumWords(const OutOfLineBits* bits) {
    return (bits->numBits + BitsInPointer - 1) / BitsInPointer;
}

std::uintptr_t* Words(BitVector& vector) {
    if (IsInline(vector)) return &vector.bitsOrPointer;
    return Words(OutOfLine(vector));
}

std::size_t Size(const BitVector& vector) {
    if (IsInline(vector)) return MaxInlineBits;
    return OutOfLine(vector)->numBits;
}

bool Get(const BitVector& vector, std::size_t bit) {
    if (bit >= Size(vector)) return false;
    const std::uintptr_t* words = IsInline(vector) ? &vector.bitsOrPointer : Words(OutOfLine(vector));
    return words[bit / BitsInPointer] & (std::uintptr_t{1} << (bit % BitsInPointer));
}

OutOfLineBits* Create(std::size_t numBits) {
    if (numBits > std::numeric_limits<std::size_t>::max() - (BitsInPointer - 1))
        throw std::length_error("WTF::BitVector size overflows");
    numBits = (numBits + BitsInPointer - 1) & ~(BitsInPointer - 1);
    std::size_t size = sizeof(OutOfLineBits) + sizeof(std::uintptr_t) * (numBits / BitsInPointer);
    return new (_ZN3WTF10fastMallocEm(size)) OutOfLineBits{numBits};
}

void Destroy(OutOfLineBits* bits) {
    _ZN3WTF8fastFreeEPv(bits);
}

void ResizeOutOfLine(BitVector& vector, std::size_t numBits) {
    OutOfLineBits* newBits = Create(numBits);
    std::size_t newNumWords = NumWords(newBits);
    if (IsInline(vector)) {
        Words(newBits)[0] = CleanseInlineBits(vector.bitsOrPointer);
        std::memset(Words(newBits) + 1, 0, (newNumWords - 1) * sizeof(std::uintptr_t));
    } else {
        OutOfLineBits* oldBits = OutOfLine(vector);
        std::size_t oldNumWords = NumWords(oldBits);
        if (numBits > oldBits->numBits) {
            std::memcpy(Words(newBits), Words(oldBits), oldNumWords * sizeof(std::uintptr_t));
            std::memset(Words(newBits) + oldNumWords, 0, (newNumWords - oldNumWords) * sizeof(std::uintptr_t));
        } else {
            std::memcpy(Words(newBits), Words(oldBits), newNumWords * sizeof(std::uintptr_t));
        }
        Destroy(oldBits);
    }
    vector.bitsOrPointer = PointerBits(newBits);
}

}  // namespace

extern "C" {

OutOfLineBits* APS5_VABI _ZN3WTF9BitVector13OutOfLineBits6createEm(std::size_t numBits) {
    return Create(numBits);
}

void APS5_VABI _ZN3WTF9BitVector6resizeEm(BitVector* self, std::size_t numBits) {
    BitVector& vector = Checked(self);
    if (numBits <= MaxInlineBits) {
        if (IsInline(vector)) return;
        OutOfLineBits* bits = OutOfLine(vector);
        vector.bitsOrPointer = MakeInlineBits(*Words(bits));
        Destroy(bits);
        return;
    }
    ResizeOutOfLine(vector, numBits);
}

void APS5_VABI _ZN3WTF9BitVector7setSlowERKS0_(BitVector* self, const BitVector* other) {
    BitVector& vector = Checked(self);
    const BitVector& source = Checked(other);
    std::uintptr_t newBitsOrPointer;
    if (IsInline(source) || IsEmptyOrDeletedValue(source)) {
        newBitsOrPointer = source.bitsOrPointer;
    } else {
        OutOfLineBits* sourceBits = OutOfLine(source);
        OutOfLineBits* newBits = Create(sourceBits->numBits);
        std::memcpy(Words(newBits), Words(sourceBits), (sourceBits->numBits + 7) / 8);
        newBitsOrPointer = PointerBits(newBits);
    }
    if (!IsInline(vector) && !IsEmptyOrDeletedValue(vector)) Destroy(OutOfLine(vector));
    vector.bitsOrPointer = newBitsOrPointer;
}

void APS5_VABI _ZN3WTF9BitVector8clearAllEv(BitVector* self) {
    BitVector& vector = Checked(self);
    if (IsInline(vector)) {
        vector.bitsOrPointer = MakeInlineBits(0);
        return;
    }
    OutOfLineBits* bits = OutOfLine(vector);
    std::memset(Words(bits), 0, (bits->numBits + 7) / 8);
}

void APS5_VABI _ZN3WTF9BitVector9mergeSlowERKS0_(BitVector* self, const BitVector* other) {
    BitVector& vector = Checked(self);
    const BitVector& source = Checked(other);
    if (IsInline(source)) {
        *Words(vector) |= CleanseInlineBits(source.bitsOrPointer);
        return;
    }
    if (Size(source) > Size(vector)) ResizeOutOfLine(vector, Size(source));
    OutOfLineBits* bits = OutOfLine(vector);
    OutOfLineBits* sourceBits = OutOfLine(source);
    for (std::size_t i = std::min(NumWords(bits), NumWords(sourceBits)); i--;)
        Words(bits)[i] |= Words(sourceBits)[i];
}

void APS5_VABI _ZN3WTF9BitVector11excludeSlowERKS0_(BitVector* self, const BitVector* other) {
    BitVector& vector = Checked(self);
    const BitVector& source = Checked(other);
    if (IsInline(source)) {
        *Words(vector) &= ~CleanseInlineBits(source.bitsOrPointer);
        return;
    }
    OutOfLineBits* sourceBits = OutOfLine(source);
    if (IsInline(vector)) {
        vector.bitsOrPointer &= ~Words(sourceBits)[0];
        vector.bitsOrPointer |= InlineFlag;
        return;
    }
    OutOfLineBits* bits = OutOfLine(vector);
    for (std::size_t i = std::min(NumWords(bits), NumWords(sourceBits)); i--;)
        Words(bits)[i] &= ~Words(sourceBits)[i];
}

bool APS5_VABI _ZNK3WTF9BitVector11isEmptySlowEv(const BitVector* self) {
    OutOfLineBits* bits = RequireOutOfLine(self);
    for (std::size_t i = NumWords(bits); i--;) {
        if (Words(bits)[i]) return false;
    }
    return true;
}

std::size_t APS5_VABI _ZNK3WTF9BitVector12bitCountSlowEv(const BitVector* self) {
    OutOfLineBits* bits = RequireOutOfLine(self);
    std::size_t result = 0;
    for (std::size_t i = NumWords(bits); i--;)
        result += std::popcount(Words(bits)[i]);
    return result;
}

std::uintptr_t APS5_VABI _ZNK3WTF9BitVector12hashSlowCaseEv(const BitVector* self) {
    OutOfLineBits* bits = RequireOutOfLine(self);
    std::uintptr_t result = 0;
    for (std::size_t i = NumWords(bits); i--;)
        result ^= Words(bits)[i];
    return result;
}

bool APS5_VABI _ZNK3WTF9BitVector14equalsSlowCaseERKS0_(const BitVector* self, const BitVector* other) {
    const BitVector& vector = Checked(self);
    const BitVector& source = Checked(other);
    if (IsInline(vector) != IsInline(source)) {
        for (std::size_t i = std::max(Size(vector), Size(source)); i--;) {
            if (Get(vector, i) != Get(source, i)) return false;
        }
        return true;
    }
    if (IsInline(vector)) return vector.bitsOrPointer == source.bitsOrPointer;
    OutOfLineBits* bits = OutOfLine(vector);
    OutOfLineBits* sourceBits = OutOfLine(source);
    std::size_t numWords = NumWords(bits);
    std::size_t sourceNumWords = NumWords(sourceBits);
    OutOfLineBits* longer = numWords < sourceNumWords ? sourceBits : bits;
    std::size_t minNumWords = std::min(numWords, sourceNumWords);
    for (std::size_t i = minNumWords; i < std::max(numWords, sourceNumWords); ++i) {
        if (Words(longer)[i]) return false;
    }
    for (std::size_t i = minNumWords; i--;) {
        if (Words(bits)[i] != Words(sourceBits)[i]) return false;
    }
    return true;
}

}
