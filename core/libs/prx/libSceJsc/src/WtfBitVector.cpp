// Derived from Source/WTF/wtf/BitVector.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceJsc/include/WtfBitVectorExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::BitVector;
using OutOfLineBits = Jsc::WtfAbi::BitVectorOutOfLineBits;

constexpr unsigned BitsInPointer = sizeof(void*) * 8;
constexpr unsigned MaxInlineBits = BitsInPointer - 1;

bool IsInline(const BitVector& vector) {
    return vector.bitsOrPointer >> MaxInlineBits;
}

bool IsEmptyOrDeletedValue(const BitVector& vector) {
    return vector.bitsOrPointer <= 1;
}

std::uintptr_t CleanseInlineBits(std::uintptr_t bits) {
    return bits & ~(static_cast<std::uintptr_t>(1) << MaxInlineBits);
}

OutOfLineBits* OutOfLine(const BitVector& vector) {
    if (IsEmptyOrDeletedValue(vector)) throw std::invalid_argument("WTF::BitVector::filterSlow: empty or deleted vector");
    return reinterpret_cast<OutOfLineBits*>(vector.bitsOrPointer << 1);
}

std::size_t NumWords(const OutOfLineBits* bits) {
    return (bits->numBits + BitsInPointer - 1) / BitsInPointer;
}

std::uintptr_t* Words(OutOfLineBits* bits) {
    return reinterpret_cast<std::uintptr_t*>(bits + 1);
}

OutOfLineBits* CreateOutOfLine(std::size_t numBits) {
    if (numBits > std::numeric_limits<std::size_t>::max() - (BitsInPointer - 1))
        throw std::length_error("WTF::BitVector::resizeOutOfLine: size overflows");
    numBits = (numBits + BitsInPointer - 1) & ~static_cast<std::size_t>(BitsInPointer - 1);
    const std::size_t size = sizeof(OutOfLineBits) + sizeof(std::uintptr_t) * (numBits / BitsInPointer);
    void* memory = malloc_nid_postfix(size);
    if (!memory) throw std::bad_alloc();
    return new (memory) OutOfLineBits{numBits};
}

}

extern "C" {

void APS5_VABI _ZN3WTF9BitVector10filterSlowERKS0_(BitVector* self, const BitVector* other) {
    if (!self || !other) throw std::invalid_argument("WTF::BitVector::filterSlow requires two vectors");
    if (IsInline(*other)) {
        if (IsInline(*self)) throw std::invalid_argument("WTF::BitVector::filterSlow: both vectors are inline");
        *Words(OutOfLine(*self)) &= CleanseInlineBits(other->bitsOrPointer);
        return;
    }

    if (IsInline(*self)) {
        self->bitsOrPointer &= *Words(OutOfLine(*other));
        self->bitsOrPointer |= (static_cast<std::uintptr_t>(1) << MaxInlineBits);
        return;
    }

    OutOfLineBits* a = OutOfLine(*self);
    OutOfLineBits* b = OutOfLine(*other);
    for (unsigned i = std::min(NumWords(a), NumWords(b)); i--;)
        Words(a)[i] &= Words(b)[i];

    for (unsigned i = NumWords(b); i < NumWords(a); ++i)
        Words(a)[i] = 0;
}

void APS5_VABI _ZN3WTF9BitVector15resizeOutOfLineEm(BitVector* self, std::size_t numBits) {
    if (!self) throw std::invalid_argument("WTF::BitVector::resizeOutOfLine requires a vector");
    if (numBits <= MaxInlineBits) throw std::invalid_argument("WTF::BitVector::resizeOutOfLine: size fits inline");
    if (!IsInline(*self) && IsEmptyOrDeletedValue(*self))
        throw std::invalid_argument("WTF::BitVector::resizeOutOfLine: empty or deleted vector");
    OutOfLineBits* newBits = CreateOutOfLine(numBits);
    const std::size_t newNumWords = NumWords(newBits);
    if (IsInline(*self)) {
        *Words(newBits) = CleanseInlineBits(self->bitsOrPointer);
        std::memset(Words(newBits) + 1, 0, (newNumWords - 1) * sizeof(std::uintptr_t));
    } else {
        OutOfLineBits* oldBits = OutOfLine(*self);
        if (numBits > oldBits->numBits) {
            const std::size_t oldNumWords = NumWords(oldBits);
            std::memcpy(Words(newBits), Words(oldBits), oldNumWords * sizeof(std::uintptr_t));
            std::memset(Words(newBits) + oldNumWords, 0, (newNumWords - oldNumWords) * sizeof(std::uintptr_t));
        } else {
            std::memcpy(Words(newBits), Words(oldBits), newNumWords * sizeof(std::uintptr_t));
        }
        free_nid_postfix(oldBits);
    }
    self->bitsOrPointer = reinterpret_cast<std::uintptr_t>(newBits) >> 1;
}

}
