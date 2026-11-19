/*
 * Derived from Source/WTF/wtf/PrintStream.cpp at https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.cpp
 *
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

#include "prx/libSceNKWebKit/include/WtfPrintfVariableFormatExports.hpp"

#include <cstdarg>
#include <stdexcept>

namespace {

using Vprintf = void (APS5_VABI*)(Jsc::WtfAbi::PrintStream*, const char*, VaList*);

Vprintf VirtualVprintf(Jsc::WtfAbi::PrintStream* stream) {
    if (!stream || !stream->vtable) throw std::invalid_argument("WTF::PrintStream::printfVariableFormat: null stream or vtable");
    const void* entry = stream->vtable[Jsc::WtfAbi::PrintStreamVprintf];
    if (!entry) throw std::invalid_argument("WTF::PrintStream::printfVariableFormat: null vprintf");
    return reinterpret_cast<Vprintf>(const_cast<void*>(entry));
}

} // namespace

extern "C" {

void APS5_VABI _ZN3WTF11PrintStream20printfVariableFormatEPKcz(Jsc::WtfAbi::PrintStream* self, const char* format, ...) {
    const Vprintf vprintf = VirtualVprintf(self);
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    vprintf(self, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
}

}
