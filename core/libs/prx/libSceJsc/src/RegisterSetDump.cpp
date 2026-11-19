/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Metrological Group B.V.
 * Copyright (C) 2019 Igalia S.L.
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
 *
 * Derived from Source/JavaScriptCore/jit/RegisterSet.cpp, jit/RegisterSet.h, jit/Reg.cpp, jit/Reg.h,
 * jit/GPRInfo.h, jit/FPRInfo.h, assembler/X86_64Registers.h, Source/WTF/wtf/Bitmap.h, wtf/CommaPrinter.h
 * and wtf/PrintStream.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
 */

#include "prx/libSceJsc/include/RegisterSetDumpExports.hpp"

#include <array>
#include <cstdarg>
#include <stdexcept>

namespace {

using Jsc::RegisterSetAbi::RegisterCount;
using Jsc::RegisterSetAbi::RegisterSet;
using Jsc::WtfAbi::PrintStream;
using Vprintf = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using Begin = PrintStream* (APS5_VABI*)(PrintStream*);
using End = void (APS5_VABI*)(PrintStream*);

constexpr std::array<const char*, RegisterCount> RegisterNames = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
};

template <typename T>
T Slot(PrintStream* stream, std::size_t slot) {
    if (!stream || !stream->vtable) throw std::invalid_argument("JSC::RegisterSet::dump: null stream or vtable");
    const void* entry = stream->vtable[slot];
    if (!entry) throw std::invalid_argument("JSC::RegisterSet::dump: null PrintStream virtual");
    return reinterpret_cast<T>(const_cast<void*>(entry));
}

void APS5_VABI Printf(PrintStream* stream, const char* format, ...) {
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

PrintStream* BeginOn(PrintStream* stream) {
    return Slot<Begin>(stream, Jsc::WtfAbi::PrintStreamBegin)(stream);
}

void EndOn(PrintStream* stream) {
    Slot<End>(stream, Jsc::WtfAbi::PrintStreamEnd)(stream);
}

void PrintText(PrintStream* out, const char* text) {
    PrintStream* begun = BeginOn(out);
    Printf(begun, "%s", text);
    EndOn(out);
}

void PrintRegister(PrintStream* out, unsigned index) {
    PrintStream* begun = BeginOn(out);
    PrintStream* named = BeginOn(begun);
    Printf(named, "%s", "%");
    Printf(named, "%s", RegisterNames[index]);
    EndOn(begun);
    EndOn(out);
}

}  // namespace

extern "C" {

void APS5_VABI _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE(const RegisterSet* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("JSC::RegisterSet::dump: null register set");
    PrintText(out, "[");
    bool didPrint = false;
    for (unsigned index = 0; index < RegisterCount; ++index) {
        if (!((self->bits >> index) & 1)) continue;
        PrintStream* begun = BeginOn(out);
        PrintText(begun, didPrint ? ", " : "");
        didPrint = true;
        PrintRegister(begun, index);
        EndOn(out);
    }
    PrintText(out, "]");
}

}
