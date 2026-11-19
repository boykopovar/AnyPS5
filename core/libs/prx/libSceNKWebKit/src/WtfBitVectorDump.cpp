/*
 * Derived from Source/WTF/wtf/BitVector.cpp at https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/BitVector.cpp
 *
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

#include "prx/libSceNKWebKit/include/WtfBitVectorDumpExports.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

using WTF::BitVector;
using OutOfLineBits = WTF::BitVector::OutOfLineBits;
using Jsc::WtfAbi::PrintStream;
using Vprintf = void (APS5_VABI*)(PrintStream*, const char*, VaList*);

constexpr std::size_t BitsInPointer = sizeof(void*) * 8;
constexpr std::size_t MaxInlineBits = BitsInPointer - 1;

Vprintf VirtualVprintf(PrintStream* stream) {
    if (!stream || !stream->vtable) throw std::invalid_argument("WTF::BitVector::dump: null stream or vtable");
    const void* entry = stream->vtable[Jsc::WtfAbi::PrintStreamVprintf];
    if (!entry) throw std::invalid_argument("WTF::BitVector::dump: null vprintf");
    return reinterpret_cast<Vprintf>(const_cast<void*>(entry));
}

void Printf(PrintStream* stream, Vprintf vprintf, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    vprintf(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
}

}  // namespace

extern "C" {

void APS5_VABI _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(const BitVector* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("WTF::BitVector::dump: null vector");
    const Vprintf vprintf = VirtualVprintf(out);
    const std::uintptr_t* words = &self->bitsOrPointer;
    std::size_t size = MaxInlineBits;
    if (!(self->bitsOrPointer >> MaxInlineBits)) {
        if (self->bitsOrPointer <= 1) throw std::invalid_argument("WTF::BitVector::dump: empty or deleted vector");
        const auto* bits = reinterpret_cast<const OutOfLineBits*>(self->bitsOrPointer << 1);
        words = reinterpret_cast<const std::uintptr_t*>(bits + 1);
        size = bits->numBits;
    }
    for (std::size_t i = 0; i < size; ++i) {
        const bool set = words[i / BitsInPointer] & (std::uintptr_t{1} << (i % BitsInPointer));
        Printf(out, vprintf, set ? "1" : "-");
    }
}

}
