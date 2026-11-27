// Derived from Source/WTF/wtf/Assertions.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2003-2019 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2011 University of Szeged. All rights reserved.
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

#include "prx/libSceJsc/include/WtfAssertionsExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vector>

extern "C" {

void APS5_VABI WTFLogAlwaysV(const char* format, VaList* args) {
    if (!format) throw std::invalid_argument("WTFLogAlwaysV: null format");
    const std::size_t length = std::strlen(format);
    if (length && format[length - 1] == '\n') {
        vfprintf_nid_postfix(&_Stderr_nid_postfix, format, args);
        return;
    }
    std::vector<char> withNewline(length + 2);
    std::memcpy(withNewline.data(), format, length);
    withNewline[length] = '\n';
    withNewline[length + 1] = 0;
    vfprintf_nid_postfix(&_Stderr_nid_postfix, withNewline.data(), args);
}

}
