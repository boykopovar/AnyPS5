// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/PrintStream.h, PrintStream.cpp, BitVector.cpp, Threading.cpp, MetaAllocator.cpp, Source/JavaScriptCore/heap/FreeList.cpp, runtime/ClassInfo.cpp, domjit/DOMJITHeapRange.cpp and Source/WebCore/platform/graphics/PlatformTimeRanges.cpp
/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfObjectDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfTimeDumpExports.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

using NKWebKit::WtfDumpAbi::FreeList;
using NKWebKit::WtfDumpAbi::MetaAllocatorHandle;
using NKWebKit::WtfDumpAbi::PrintStream;

using VprintfFunction = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using BeginFunction = PrintStream* (APS5_VABI*)(PrintStream*);
using EndFunction = void (APS5_VABI*)(PrintStream*);

constexpr std::size_t BitsInWord = sizeof(std::uintptr_t) * 8;
constexpr std::size_t MaxInlineBits = BitsInWord - 1;
constexpr std::uintptr_t InlineFlag = std::uintptr_t{1} << MaxInlineBits;

template<typename TFunction>
TFunction Slot(PrintStream& out, std::size_t slot) {
    if (!out.vtable) throw std::invalid_argument("WTF::PrintStream without a vtable");
    return reinterpret_cast<TFunction>(const_cast<void*>(out.vtable[slot]));
}

void APS5_VABI Printf_nid_no_patch(PrintStream& out, const char* format, ...) {
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

void PrintInternal(PrintStream& out, const char* string) {
    Printf_nid_no_patch(out, "%s", string);
}

void PrintInternal(PrintStream& out, const void* pointer) {
    Printf_nid_no_patch(out, "%p", pointer);
}

void PrintInternal(PrintStream& out, unsigned value) {
    Printf_nid_no_patch(out, "%u", value);
}

void PrintInternal(PrintStream& out, std::uint64_t value) {
    Printf_nid_no_patch(out, "%lu", value);
}

void PrintInternal(PrintStream& out, const NKWebKit::HttpAbi::ASCIILiteral& value) {
    _ZNK3WTF12ASCIILiteral4dumpERNS_11PrintStreamE(&value, &out);
}

void PrintInternal(PrintStream& out, const WTF::MediaTime& value) {
    _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(&value, &out);
}

template<typename... TValues>
void Print(PrintStream& out, const TValues&... values) {
    PrintStream* stream = Slot<BeginFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamBegin)(&out);
    if (!stream) throw std::invalid_argument("WTF::PrintStream::begin returned null");
    (PrintInternal(*stream, values), ...);
    Slot<EndFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamEnd)(&out);
}

bool BitAt(const WTF::BitVector& vector, std::size_t index) {
    if (vector.bitsOrPointer & InlineFlag) return (vector.bitsOrPointer >> index) & 1;
    const auto* bits = reinterpret_cast<const WTF::BitVector::OutOfLineBits*>(vector.bitsOrPointer << 1);
    const auto* words = reinterpret_cast<const std::uintptr_t*>(bits + 1);
    return (words[index / BitsInWord] >> (index % BitsInWord)) & 1;
}

std::size_t BitCount(const WTF::BitVector& vector) {
    if (vector.bitsOrPointer & InlineFlag) return MaxInlineBits;
    if (vector.bitsOrPointer <= 1) throw std::invalid_argument("WTF::BitVector::dump on an empty or deleted hash table value");
    return reinterpret_cast<const WTF::BitVector::OutOfLineBits*>(vector.bitsOrPointer << 1)->numBits;
}

}  // namespace

extern "C" {

void APS5_VABI _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(const NKWebKit::WtfDumpAbi::HeapRange* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    Printf_nid_no_patch(*out, "0x%x-0x%x", static_cast<unsigned>(self->begin), static_cast<unsigned>(self->end));
}

void APS5_VABI _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(const NKWebKit::WtfDumpAbi::ClassInfo* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    const void* pointer = self;
    Print(*out, pointer, ":[", self->className, "]");
}

void APS5_VABI _ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE(const FreeList* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    const void* nextInterval = self->nextInterval;
    const void* intervalStart = self->intervalStart;
    const void* intervalEnd = self->intervalEnd;
    Print(*out, "{nextInterval = ", nextInterval, ", secret = ", self->secret, ", intervalStart = ", intervalStart,
          ", intervalEnd = ", intervalEnd, ", originalSize = ", self->originalSize, "}");
}

void APS5_VABI _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(const MetaAllocatorHandle* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    const void* start = self->start;
    const void* end = self->end;
    Print(*out, start, "...", end);
}

void APS5_VABI _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(const void* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    Print(*out, "Thread:", self);
}

void APS5_VABI _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(const WTF::BitVector* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    const std::size_t size = BitCount(*self);
    for (std::size_t i = 0; i < size; ++i) Print(*out, BitAt(*self, i) ? "1" : "-");
}

void APS5_VABI _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(const WebCore::PlatformTimeRanges* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    const auto& ranges = self->m_ranges;
    if (ranges.m_size && !ranges.m_buffer) throw std::invalid_argument("WebCore::PlatformTimeRanges without range storage");
    for (unsigned i = 0; i < ranges.m_size; ++i) Print(*out, "[", ranges.m_buffer[i].start, "..", ranges.m_buffer[i].end, "] ");
}

}
