// Derived from Source/JavaScriptCore/runtime/ScriptExecutable.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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

// Derived from Source/JavaScriptCore/runtime/FunctionExecutable.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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

// Derived from Source/JavaScriptCore/runtime/GlobalExecutable.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

// Derived from Source/JavaScriptCore/bytecode/UnlinkedFunctionExecutable.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2012-2021 Apple Inc. All Rights Reserved.
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

#include "prx/libSceNKWebKit/include/ScriptExecutableExports.hpp"
#include "prx/libSceNKWebKit/src/ScriptExecutableAbi.hpp"

#include <cstring>
#include <stdexcept>

namespace {

using namespace NKWebKit::JscAbi;

const unsigned char* Checked(const void* pointer, const char* what) {
    if (!pointer) throw std::invalid_argument(what);
    return static_cast<const unsigned char*>(pointer);
}

std::uint32_t ReadU32(const unsigned char* base, std::size_t offset) {
    std::uint32_t value;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

const unsigned char* ReadPointer(const unsigned char* base, std::size_t offset) {
    const unsigned char* value;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

bool IsFunctionExecutable(const unsigned char* executable) {
    return executable[kCellTypeOffset] == kFunctionExecutableType;
}

const unsigned char* UnlinkedExecutable(const unsigned char* executable) {
    return Checked(ReadPointer(executable, kFunctionExecutableUnlinkedOffset), "null JSC::UnlinkedFunctionExecutable");
}

std::uint32_t UnlinkedLineCount(const unsigned char* unlinked) {
    return ReadU32(unlinked, kUnlinkedLineCountOffset) & kUnlinkedBitfieldMask;
}

}

extern "C" {

int APS5_VABI _ZNK3JSC16ScriptExecutable8lastLineEv(const void* self) {
    const unsigned char* executable = Checked(self, "null JSC::ScriptExecutable");
    if (!IsFunctionExecutable(executable))
        return static_cast<int>(ReadU32(executable, kGlobalExecutableLastLineOffset));

    const std::uint32_t firstLine = ReadU32(executable, kSourceFirstLineOffset) + 1;
    const unsigned char* rareData = ReadPointer(executable, kFunctionExecutableRareDataOffset);
    const std::uint32_t lineCount = rareData ? ReadU32(rareData, kRareDataLineCountOffset)
                                             : UnlinkedLineCount(UnlinkedExecutable(executable));
    return static_cast<int>(firstLine + lineCount);
}

unsigned APS5_VABI _ZNK3JSC16ScriptExecutable9endColumnEv(const void* self) {
    const unsigned char* executable = Checked(self, "null JSC::ScriptExecutable");
    if (!IsFunctionExecutable(executable))
        return ReadU32(executable, kGlobalExecutableEndColumnOffset);

    const unsigned char* rareData = ReadPointer(executable, kFunctionExecutableRareDataOffset);
    if (rareData)
        return ReadU32(rareData, kRareDataEndColumnOffset);

    const unsigned char* unlinked = UnlinkedExecutable(executable);
    const std::uint32_t startColumn = ReadU32(executable, kSourceStartColumnOffset) + 1;
    const std::uint32_t linkedColumn = UnlinkedLineCount(unlinked) ? 1 : startColumn;
    return (ReadU32(unlinked, kUnlinkedBodyEndColumnOffset) & kUnlinkedBitfieldMask) + linkedColumn;
}

}
