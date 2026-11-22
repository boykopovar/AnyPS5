// Derived from Source/JavaScriptCore/heap/CellAttributes.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/JavaScriptCore/heap/DestructionMode.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/JavaScriptCore/heap/HeapCell.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/JavaScriptCore/domjit/DOMJITHeapRange.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/JscDumpExports.hpp"

#include <cstdarg>
#include <cstddef>
#include <stdexcept>

namespace {

using NKWebKit::JscDumpAbi::CellAttributes;
using NKWebKit::JscDumpAbi::HeapRange;
using NKWebKit::WtfDumpAbi::PrintStream;

using VprintfFunction = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using BeginFunction = PrintStream* (APS5_VABI*)(PrintStream*);
using EndFunction = void (APS5_VABI*)(PrintStream*);

template<typename TFunction>
TFunction Slot(PrintStream& out, std::size_t slot) {
    return reinterpret_cast<TFunction>(const_cast<void*>(out.vtable[slot]));
}

void APS5_VABI JscDumpPrintf_nid_no_patch(PrintStream& out, const char* format, ...) {
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

void PrintText(PrintStream& out, const char* text) {
    JscDumpPrintf_nid_no_patch(out, "%s", text);
}

void Print(PrintStream& out, const char* text) {
    PrintStream& stream = Begin(out);
    PrintText(stream, text);
    End(out);
}

const char* DestructionModeName(std::int8_t mode) {
    switch (mode) {
    case 0:
        return "DoesNotNeedDestruction";
    case 1:
        return "NeedsDestruction";
    }
    throw std::invalid_argument("invalid JSC::DestructionMode");
}

const char* CellKindName(std::int8_t kind) {
    switch (kind) {
    case 0:
        return "JSCell";
    case 1:
        return "JSCellWithIndexingHeader";
    case 2:
        return "Auxiliary";
    }
    throw std::invalid_argument("invalid JSC::HeapCell::Kind");
}

}

extern "C" {

void APS5_VABI _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(const CellAttributes* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("null JSC::CellAttributes");
    PrintStream& outer = Checked(out);
    const char* destruction = DestructionModeName(self->destruction);
    const char* cellKind = CellKindName(self->cellKind);
    PrintStream& stream = Begin(outer);
    PrintText(stream, "{");
    Print(stream, destruction);
    PrintText(stream, ", ");
    Print(stream, cellKind);
    PrintText(stream, "}");
    End(outer);
}

void APS5_VABI _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(const HeapRange* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("null JSC::DOMJIT::HeapRange");
    JscDumpPrintf_nid_no_patch(Checked(out), "0x%x-0x%x", static_cast<unsigned>(self->begin), static_cast<unsigned>(self->end));
}

}
