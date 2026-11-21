// Derived from Source/JavaScriptCore/bytecode/CodeOrigin.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/JavaScriptCore/bytecode/CodeOrigin.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/JavaScriptCore/bytecode/BytecodeIndex.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/JavaScriptCore/runtime/CacheableIdentifier.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/JavaScriptCore/runtime/CacheableIdentifierInlines.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WTF/wtf/PrintStream.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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
* Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include "prx/libSceNKWebKit/include/JscBytecodeDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

using NKWebKit::JscBytecodeDumpAbi::CacheableIdentifier;
using NKWebKit::JscBytecodeDumpAbi::CodeOrigin;
using NKWebKit::JscBytecodeDumpAbi::OutOfLineCodeOrigin;
using NKWebKit::WtfAbi::ExpectedCString;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfDumpAbi::PrintStream;

using VprintfFunction = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using BeginFunction = PrintStream* (APS5_VABI*)(PrintStream*);
using EndFunction = void (APS5_VABI*)(PrintStream*);

constexpr std::uintptr_t CodeOriginOutOfLineBit = 1;
constexpr std::uintptr_t CodeOriginInvalidBytecodeIndexBit = 2;
constexpr unsigned EffectiveAddressWidth = 48;
constexpr unsigned CodeOriginFreeBitsAtTop = 64 - EffectiveAddressWidth;
constexpr std::uintptr_t CodeOriginPointerMask = ((std::uintptr_t{1} << EffectiveAddressWidth) - 1) & ~std::uintptr_t{7};
constexpr std::uint32_t BytecodeIndexCheckpointShift = 2;
constexpr std::uint32_t BytecodeIndexCheckpointMask = 3;
constexpr std::uintptr_t CacheableIdentifierUidTag = 1;
constexpr std::uint32_t Utf8ConversionOutOfMemory = 1;
constexpr unsigned StringImpl8BitFlag = 4;

template<typename TFunction>
TFunction Slot(PrintStream& out, std::size_t slot) {
    return reinterpret_cast<TFunction>(const_cast<void*>(out.vtable[slot]));
}

void APS5_VABI JscBytecodeDumpPrintf_nid_no_patch(PrintStream& out, const char* format, ...) {
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
    JscBytecodeDumpPrintf_nid_no_patch(out, "%s", text);
}

void PrintUnsigned(PrintStream& out, unsigned value) {
    JscBytecodeDumpPrintf_nid_no_patch(out, "%u", value);
}

void Print(PrintStream& out, const char* text) {
    PrintStream& stream = Begin(out);
    PrintText(stream, text);
    End(out);
}

void PrintTextAndUnsigned(PrintStream& out, const char* text, unsigned value) {
    PrintStream& stream = Begin(out);
    PrintText(stream, text);
    PrintUnsigned(stream, value);
    End(out);
}

void DumpBytecodeIndex(PrintStream& out, std::uint32_t bits) {
    PrintTextAndUnsigned(out, "bc#", bits >> BytecodeIndexCheckpointShift);
    if (const std::uint32_t checkpoint = bits & BytecodeIndexCheckpointMask)
        PrintTextAndUnsigned(out, "cp#", checkpoint);
}

std::uint32_t NonInlinedBytecodeIndex(std::uintptr_t compositeValue) {
    const void* inlineCallFrame;
    std::uint32_t bits;
    if (compositeValue & CodeOriginOutOfLineBit) {
        const auto* outOfLine = reinterpret_cast<const OutOfLineCodeOrigin*>(compositeValue & CodeOriginPointerMask);
        if (!outOfLine) throw std::invalid_argument("null JSC::CodeOrigin out-of-line storage");
        inlineCallFrame = outOfLine->inlineCallFrame;
        bits = outOfLine->bytecodeIndex;
    } else {
        inlineCallFrame = reinterpret_cast<const void*>(compositeValue & CodeOriginPointerMask);
        bits = static_cast<std::uint32_t>(compositeValue >> (64 - CodeOriginFreeBitsAtTop));
    }
    if (inlineCallFrame) throw std::invalid_argument("inlined JSC::CodeOrigin is unsupported");
    return bits;
}

ExpectedCString StringImplUtf8(const StringImpl& string) {
    if (string.hashAndFlags & StringImpl8BitFlag)
        return _ZN3WTF10StringImpl17utf8ForCharactersEPKhj(static_cast<const unsigned char*>(string.data), string.length);
    return _ZN3WTF10StringImpl17utf8ForCharactersEPKDsjNS_14ConversionModeE(static_cast<const char16_t*>(string.data), string.length, 0);
}

const char* CStringData(const ExpectedCString& converted) {
    return converted.value.buffer ? reinterpret_cast<const char*>(converted.value.buffer + 1) : nullptr;
}

void PrintStringImpl(PrintStream& out, const StringImpl* string, const ExpectedCString* converted) {
    if (!string) {
        PrintText(out, "(null StringImpl*)");
        return;
    }
    if (!converted->has) {
        PrintText(out, converted->error == Utf8ConversionOutOfMemory ? "(Out of memory while converting " : "(failed to convert ");
        PrintText(out, "StringImpl*");
        PrintText(out, " to utf8)");
        return;
    }
    PrintText(out, CStringData(*converted));
}

}

extern "C" {

void APS5_VABI _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(const CodeOrigin* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("null JSC::CodeOrigin");
    PrintStream& outer = Checked(out);
    const std::uintptr_t compositeValue = self->compositeValue;
    if (compositeValue & CodeOriginInvalidBytecodeIndexBit) {
        Print(outer, "<none>");
        return;
    }
    const std::uint32_t bits = NonInlinedBytecodeIndex(compositeValue);
    PrintStream& stream = Begin(outer);
    DumpBytecodeIndex(stream, bits);
    End(outer);
}

void APS5_VABI _ZNK3JSC19CacheableIdentifier4dumpERN3WTF11PrintStreamE(const CacheableIdentifier* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("null JSC::CacheableIdentifier");
    PrintStream& outer = Checked(out);
    const std::uintptr_t bits = self->bits;
    if (!bits) {
        Print(outer, "");
        return;
    }
    if (!(bits & CacheableIdentifierUidTag)) throw std::invalid_argument("JSC::CacheableIdentifier cell is unsupported");
    const auto* uid = reinterpret_cast<const StringImpl*>(bits & ~CacheableIdentifierUidTag);
    const ExpectedCString converted = uid ? StringImplUtf8(*uid) : ExpectedCString(Utf8ConversionOutOfMemory);
    PrintStream& stream = Begin(outer);
    PrintText(stream, "uid:(");
    PrintStringImpl(stream, uid, &converted);
    PrintText(stream, ")");
    End(outer);
}

}
