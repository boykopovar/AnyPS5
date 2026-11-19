/*
 * Derived from Source/JavaScriptCore/bytecode/CodeOrigin.cpp, Source/JavaScriptCore/bytecode/CodeOrigin.h,
 * Source/JavaScriptCore/bytecode/BytecodeIndex.cpp and Source/WTF/wtf/PrintStream.cpp
 * at https://github.com/WebKit/WebKit/tree/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source
 *
 * CodeOrigin.cpp:
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
 *
 * CodeOrigin.h:
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
 *
 * BytecodeIndex.cpp:
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * PrintStream.cpp:
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/JscCodeOriginDumpExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdarg>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::PrintStream;
using namespace NKWebKit::JscCodeOriginAbi;
using Vprintf = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using Begin = PrintStream* (APS5_VABI*)(PrintStream*);
using End = void (APS5_VABI*)(PrintStream*);

template <typename T>
T Slot(PrintStream* stream, std::size_t slot) {
    if (!stream || !stream->vtable) throw std::invalid_argument("JSC::CodeOrigin::dump: null stream or vtable");
    const void* entry = stream->vtable[slot];
    if (!entry) throw std::invalid_argument("JSC::CodeOrigin::dump: null PrintStream virtual");
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

PrintStream* BeginPrint(PrintStream* stream) {
    return Slot<Begin>(stream, Jsc::WtfAbi::PrintStreamBegin)(stream);
}

void EndPrint(PrintStream* stream) {
    Slot<End>(stream, Jsc::WtfAbi::PrintStreamEnd)(stream);
}

void PrintLabelledNumber(PrintStream* stream, const char* label, unsigned value) {
    Slot<End>(stream, Jsc::WtfAbi::PrintStreamEnd);
    PrintStream* begun = BeginPrint(stream);
    Printf(begun, "%s", label);
    Printf(begun, "%u", value);
    EndPrint(stream);
}

void DumpBytecodeIndex(PrintStream* stream, std::uint32_t bits) {
    PrintLabelledNumber(stream, "bc#", bits >> CheckpointShift);
    if (const std::uint32_t checkpoint = bits & CheckpointMask) PrintLabelledNumber(stream, "cp#", checkpoint);
}

}  // namespace

extern "C" {

void APS5_VABI _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(const CodeOrigin* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("JSC::CodeOrigin::dump: null code origin");
    Slot<End>(out, Jsc::WtfAbi::PrintStreamEnd);
    Slot<Begin>(out, Jsc::WtfAbi::PrintStreamBegin);
    const std::uintptr_t value = self->compositeValue;
    if (value & MaskIsBytecodeIndexInvalid) {
        PrintStream* begun = BeginPrint(out);
        Printf(begun, "%s", "<none>");
        EndPrint(out);
        return;
    }
    const void* inlineCallFrame = nullptr;
    std::uint32_t bits = 0;
    if (value & MaskIsOutOfLine) {
        const auto* outOfLine = reinterpret_cast<const OutOfLineCodeOrigin*>(value & MaskCompositeValueForPointer);
        if (!outOfLine) throw std::invalid_argument("JSC::CodeOrigin::dump: null out-of-line code origin");
        inlineCallFrame = outOfLine->inlineCallFrame;
        bits = outOfLine->bytecodeIndex;
    } else {
        inlineCallFrame = reinterpret_cast<const void*>(value & MaskCompositeValueForPointer);
        bits = static_cast<std::uint32_t>(value >> (64 - FreeBitsAtTop));
    }
    if (inlineCallFrame) NotImplemented_nid_no_patch(__func__);
    PrintStream* begun = BeginPrint(out);
    DumpBytecodeIndex(begun, bits);
    EndPrint(out);
}

}
