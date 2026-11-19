// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/PrintStream.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/JavaScriptCore/runtime/JSArrayBufferView.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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
/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WtfPrintStreamExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <stdexcept>

namespace {

using NKWebKit::WtfAbi::PrintStream;
using NKWebKit::WtfAbi::PrintStreamSlot;
using NKWebKit::WtfAbi::StringView;

using PrintVprintf = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using PrintBegin = PrintStream* (APS5_VABI*)(PrintStream*);
using PrintEnd = void (APS5_VABI*)(PrintStream*);

constexpr unsigned MaxStringLength = 0x7FFFFFFF;

void* SlotOf(PrintStream& out, PrintStreamSlot slot) {
    if (!out.vtable || !out.vtable[slot])
        throw std::invalid_argument("WTF::PrintStream: null stream or vtable");
    return const_cast<void*>(out.vtable[slot]);
}

void PrintChars(PrintStream& out, const char* text) {
    const auto printSlot = reinterpret_cast<PrintVprintf>(SlotOf(out, PrintStreamSlot::PrintStreamVprintf));
    const char* argument = text;
    VaList list{};
    list.gp_offset = 48;
    list.fp_offset = 176;
    list.overflow_arg_area = &argument;
    list.reg_save_area = nullptr;
    printSlot(&out, "%s", &list);
}

template<typename Print>
void PrintAtomically(PrintStream& out, Print&& print) {
    const auto begin = reinterpret_cast<PrintBegin>(SlotOf(out, PrintStreamSlot::PrintStreamBegin));
    const auto end = reinterpret_cast<PrintEnd>(SlotOf(out, PrintStreamSlot::PrintStreamEnd));
    PrintStream* inner = begin(&out);
    if (!inner || !inner->vtable || !inner->vtable[PrintStreamSlot::PrintStreamVprintf]) {
        end(&out);
        throw std::invalid_argument("WTF::PrintStream: begin returned no printable stream");
    }
    print(*inner);
    end(&out);
}

struct Utf8Text {
    NKWebKit::WtfAbi::CString converted;
    bool outOfMemory;
};

Utf8Text ConvertUtf8(const StringView& view) {
    if ((!view.is8Bit && view.length > MaxStringLength / 3) || (view.is8Bit && view.length > MaxStringLength / 2))
        return {NKWebKit::WtfAbi::CString{}, true};
    return {_ZNK3WTF10StringView4utf8ENS_14ConversionModeE(&view, 0), false};
}

void PrintUtf8Text(PrintStream& out, const Utf8Text& text, const char* type) {
    if (text.outOfMemory) {
        PrintChars(out, "(Out of memory while converting ");
        PrintChars(out, type);
        PrintChars(out, " to utf8)");
        return;
    }
    PrintChars(out, text.converted.buffer ? reinterpret_cast<const char*>(text.converted.buffer + 1) : "");
}

}  // namespace

namespace NKWebKit::WtfAbi {

void WtfPrintUtf8_nid_no_patch(PrintStream& out, const StringView& view, const char* type) {
    const Utf8Text text = ConvertUtf8(view);
    PrintUtf8Text(out, text, type);
}

void WtfPrintUtf8Atomically_nid_no_patch(PrintStream& out, const StringView& view, const char* type) {
    const Utf8Text text = ConvertUtf8(view);
    PrintAtomically(out, [&](PrintStream& inner) { PrintUtf8Text(inner, text, type); });
}

}  // namespace NKWebKit::WtfAbi

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(NKWebKit::WtfAbi::PrintStream& out, JSC::TypedArrayMode mode) {
    const char* name;
    switch (mode) {
    case JSC::TypedArrayMode::FastTypedArray:
        name = "FastTypedArray";
        break;
    case JSC::TypedArrayMode::OversizeTypedArray:
        name = "OversizeTypedArray";
        break;
    case JSC::TypedArrayMode::WastefulTypedArray:
        name = "WastefulTypedArray";
        break;
    case JSC::TypedArrayMode::ResizableNonSharedWastefulTypedArray:
        name = "ResizableNonSharedWastefulTypedArray";
        break;
    case JSC::TypedArrayMode::ResizableNonSharedAutoLengthWastefulTypedArray:
        name = "ResizableNonSharedAutoLengthWastefulTypedArray";
        break;
    case JSC::TypedArrayMode::GrowableSharedWastefulTypedArray:
        name = "GrowableSharedWastefulTypedArray";
        break;
    case JSC::TypedArrayMode::GrowableSharedAutoLengthWastefulTypedArray:
        name = "GrowableSharedAutoLengthWastefulTypedArray";
        break;
    case JSC::TypedArrayMode::DataViewMode:
        name = "DataViewMode";
        break;
    case JSC::TypedArrayMode::ResizableNonSharedDataViewMode:
        name = "ResizableNonSharedDataViewMode";
        break;
    case JSC::TypedArrayMode::ResizableNonSharedAutoLengthDataViewMode:
        name = "ResizableNonSharedAutoLengthDataViewMode";
        break;
    case JSC::TypedArrayMode::GrowableSharedDataViewMode:
        name = "GrowableSharedDataViewMode";
        break;
    case JSC::TypedArrayMode::GrowableSharedAutoLengthDataViewMode:
        name = "GrowableSharedAutoLengthDataViewMode";
        break;
    default:
        throw std::invalid_argument("WTF::printInternal: invalid JSC::TypedArrayMode");
    }
    PrintAtomically(out, [&](PrintStream& inner) { PrintChars(inner, name); });
}

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(NKWebKit::WtfAbi::PrintStream& out, const NKWebKit::WtfAbi::StringImpl* string) {
    if (!string) {
        PrintChars(out, "(null StringImpl*)");
        return;
    }
    WtfPrintUtf8_nid_no_patch(out, NKWebKit::WtfAbi::ViewOf(string), "StringImpl*");
}

}
