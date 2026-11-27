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
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/JscDumpExports.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {

using NKWebKit::JscDumpAbi::CellAttributes;
using NKWebKit::JscDumpAbi::HeapRange;
using NKWebKit::WtfDumpAbi::PrintStream;

using VprintfFunction = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using BeginFunction = PrintStream* (APS5_VABI*)(PrintStream*);
using EndFunction = void (APS5_VABI*)(PrintStream*);

static_assert(std::is_trivially_copyable_v<CellAttributes>);
static_assert(std::is_trivially_copyable_v<HeapRange>);

template<typename TFunction>
TFunction ReadSlot(PrintStream* out, std::size_t slot, const char* function) {
    if (!out) throw std::invalid_argument("null WTF::PrintStream");
    void* const* vtablePtr = nullptr;
    if (!GuestCopyIn_nid_no_patch(&vtablePtr, out, sizeof(vtablePtr)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!vtablePtr) throw std::invalid_argument("null WTF::PrintStream");
    const std::size_t byteOffset = slot * sizeof(void*);
    const auto baseAddress = reinterpret_cast<std::uintptr_t>(vtablePtr);
    if (byteOffset > std::numeric_limits<std::uintptr_t>::max() - baseAddress)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    void* slotValue = nullptr;
    if (!GuestCopyIn_nid_no_patch(&slotValue, reinterpret_cast<const void*>(baseAddress + byteOffset), sizeof(slotValue)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!slotValue) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return reinterpret_cast<TFunction>(slotValue);
}

void APS5_VABI JscDumpPrintf_nid_no_patch(PrintStream* out, VprintfFunction vprintfFn, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    vprintfFn(out, format, reinterpret_cast<VaList*>(arguments));
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    vprintfFn(out, format, reinterpret_cast<VaList*>(arguments));
    va_end(arguments);
#endif
}

PrintStream* CheckedBegin(PrintStream* out, BeginFunction beginFn, const char* function) {
    PrintStream* inner = beginFn(out);
    static_cast<void>(ReadSlot<VprintfFunction>(inner, NKWebKit::WtfDumpAbi::PrintStreamVprintf, function));
    return inner;
}

void PrintText(PrintStream* out, const char* text, const char* function) {
    const auto vprintfFn = ReadSlot<VprintfFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamVprintf, function);
    JscDumpPrintf_nid_no_patch(out, vprintfFn, "%s", text);
}

void Print(PrintStream* out, const char* text, const char* function) {
    const auto beginFn = ReadSlot<BeginFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamBegin, function);
    const auto endFn = ReadSlot<EndFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamEnd, function);
    PrintStream* stream = CheckedBegin(out, beginFn, function);
    PrintText(stream, text, function);
    endFn(out);
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

}  // namespace

extern "C" {

void APS5_VABI _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(const CellAttributes* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("null JSC::CellAttributes");
    CellAttributes local{};
    if (!GuestCopyIn_nid_no_patch(&local, self, sizeof(local)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const auto beginFn = ReadSlot<BeginFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamBegin, __func__);
    const auto endFn = ReadSlot<EndFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamEnd, __func__);
    const char* destruction = DestructionModeName(local.destruction);
    const char* cellKind = CellKindName(local.cellKind);
    PrintStream* stream = CheckedBegin(out, beginFn, __func__);
    PrintText(stream, "{", __func__);
    Print(stream, destruction, __func__);
    PrintText(stream, ", ", __func__);
    Print(stream, cellKind, __func__);
    PrintText(stream, "}", __func__);
    endFn(out);
}

void APS5_VABI _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(const HeapRange* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("null JSC::DOMJIT::HeapRange");
    HeapRange local{};
    if (!GuestCopyIn_nid_no_patch(&local, self, sizeof(local)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const auto vprintfFn = ReadSlot<VprintfFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamVprintf, __func__);
    JscDumpPrintf_nid_no_patch(out, vprintfFn, "0x%x-0x%x", static_cast<unsigned>(local.begin), static_cast<unsigned>(local.end));
}

}
