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

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

}
