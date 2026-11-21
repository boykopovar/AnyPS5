// Derived from Source/WTF/wtf/BitVector.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WTF/wtf/Threading.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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

/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfObjectDumpExports.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

using NKWebKit::WtfDumpAbi::PrintStream;
using WTF::BitVector;

using VprintfFunction = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using BeginFunction = PrintStream* (APS5_VABI*)(PrintStream*);
using EndFunction = void (APS5_VABI*)(PrintStream*);

constexpr std::size_t BitsInPointer = sizeof(void*) * 8;
constexpr std::size_t MaxInlineBits = BitsInPointer - 1;

template<typename TFunction>
TFunction Slot(PrintStream& out, std::size_t slot) {
    return reinterpret_cast<TFunction>(const_cast<void*>(out.vtable[slot]));
}

void APS5_VABI ObjectDumpPrintf_nid_no_patch(PrintStream& out, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    Slot<VprintfFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamVprintf)(&out, format, reinterpret_cast<VaList*>(arguments));
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    Slot<VprintfFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamVprintf)(&out, format, reinterpret_cast<VaList*>(arguments));
    va_end(arguments);
#endif
}

PrintStream& Checked(PrintStream* out) {
    if (!out || !out->vtable) throw std::invalid_argument("null WTF::PrintStream");
    return *out;
}

PrintStream& Begin(PrintStream& out) {
    return Checked(Slot<BeginFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamBegin)(&out));
}

void End(PrintStream& out) {
    Slot<EndFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamEnd)(&out);
}

bool IsInline(const BitVector& vector) {
    return vector.bitsOrPointer >> MaxInlineBits;
}

const BitVector::OutOfLineBits& OutOfLine(const BitVector& vector) {
    if (vector.bitsOrPointer <= 1) throw std::invalid_argument("WTF::BitVector hash table sentinel has no bits");
    return *reinterpret_cast<const BitVector::OutOfLineBits*>(vector.bitsOrPointer << 1);
}

std::size_t Size(const BitVector& vector) {
    return IsInline(vector) ? MaxInlineBits : OutOfLine(vector).numBits;
}

bool Get(const BitVector& vector, std::size_t bit) {
    if (IsInline(vector)) return (vector.bitsOrPointer >> bit) & 1;
    const auto* words = reinterpret_cast<const std::uintptr_t*>(&OutOfLine(vector) + 1);
    return (words[bit / BitsInPointer] >> (bit % BitsInPointer)) & 1;
}

}

extern "C" {

void APS5_VABI _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(const BitVector* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("null WTF::BitVector");
    PrintStream& outer = Checked(out);
    const std::size_t size = Size(*self);
    for (std::size_t bit = 0; bit < size; ++bit) {
        PrintStream& stream = Begin(outer);
        ObjectDumpPrintf_nid_no_patch(stream, "%s", Get(*self, bit) ? "1" : "-");
        End(outer);
    }
}

void APS5_VABI _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(const NKWebKit::WtfAbi::Thread* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("null WTF::Thread");
    PrintStream& outer = Checked(out);
    PrintStream& stream = Begin(outer);
    ObjectDumpPrintf_nid_no_patch(stream, "%s", "Thread:");
    ObjectDumpPrintf_nid_no_patch(stream, "%p", static_cast<const void*>(self));
    End(outer);
}

}
