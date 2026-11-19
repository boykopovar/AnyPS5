/*
 * Derived from Source/JavaScriptCore/heap/CellAttributes.cpp, Source/JavaScriptCore/heap/DestructionMode.cpp,
 * Source/JavaScriptCore/heap/HeapCell.cpp and Source/WTF/wtf/PrintStream.cpp
 * at https://github.com/WebKit/WebKit/tree/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source
 *
 * CellAttributes.cpp: Copyright (C) 2016-2018 Apple Inc. All rights reserved.
 * DestructionMode.cpp: Copyright (C) 2016 Apple Inc. All rights reserved.
 * HeapCell.cpp: Copyright (C) 2016-2020 Apple Inc. All rights reserved.
 * PrintStream.cpp: Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/JscCellAttributesDumpExports.hpp"

#include <cstdarg>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::PrintStream;
using NKWebKit::JscCellAttributesAbi::CellAttributes;
using NKWebKit::JscCellAttributesAbi::DestructionMode;
using NKWebKit::JscCellAttributesAbi::HeapCellKind;
using Vprintf = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using Begin = PrintStream* (APS5_VABI*)(PrintStream*);
using End = void (APS5_VABI*)(PrintStream*);

template <typename T>
T Slot(PrintStream* stream, std::size_t slot) {
    if (!stream || !stream->vtable) throw std::invalid_argument("JSC::CellAttributes::dump: null stream or vtable");
    const void* entry = stream->vtable[slot];
    if (!entry) throw std::invalid_argument("JSC::CellAttributes::dump: null PrintStream virtual");
    return reinterpret_cast<T>(const_cast<void*>(entry));
}

void Printf(PrintStream* stream, const char* format, ...) {
    const Vprintf vprintf = Slot<Vprintf>(stream, Jsc::WtfAbi::PrintStreamVprintf);
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

void Print(PrintStream* stream, const char* text) {
    const End end = Slot<End>(stream, Jsc::WtfAbi::PrintStreamEnd);
    PrintStream* begun = Slot<Begin>(stream, Jsc::WtfAbi::PrintStreamBegin)(stream);
    Printf(begun, "%s", text);
    end(stream);
}

const char* DestructionModeName(DestructionMode mode) {
    switch (mode) {
    case NKWebKit::JscCellAttributesAbi::NeedsDestruction:
        return "NeedsDestruction";
    case NKWebKit::JscCellAttributesAbi::DoesNotNeedDestruction:
        return "DoesNotNeedDestruction";
    }
    throw std::invalid_argument("JSC::CellAttributes::dump: invalid DestructionMode");
}

const char* HeapCellKindName(HeapCellKind kind) {
    switch (kind) {
    case NKWebKit::JscCellAttributesAbi::JSCell:
        return "JSCell";
    case NKWebKit::JscCellAttributesAbi::JSCellWithIndexingHeader:
        return "JSCellWithIndexingHeader";
    case NKWebKit::JscCellAttributesAbi::Auxiliary:
        return "Auxiliary";
    }
    throw std::invalid_argument("JSC::CellAttributes::dump: invalid HeapCell::Kind");
}

}  // namespace

extern "C" {

void APS5_VABI _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(const CellAttributes* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("JSC::CellAttributes::dump: null cell attributes");
    const End end = Slot<End>(out, Jsc::WtfAbi::PrintStreamEnd);
    PrintStream* begun = Slot<Begin>(out, Jsc::WtfAbi::PrintStreamBegin)(out);
    Printf(begun, "%s", "{");
    Print(begun, DestructionModeName(self->destruction));
    Printf(begun, "%s", ", ");
    Print(begun, HeapCellKindName(self->cellKind));
    Printf(begun, "%s", "}");
    end(out);
}

}
