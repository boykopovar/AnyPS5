/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
 * Derived from Source/WebCore/PAL/pal/text/TextEncoding.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace WebCore {

enum class UnencodableHandling : bool {
    Entities,
    URLEncodedEntities
};

struct TextEncoding {
    const void* vptr;
    const char* name;
    char16_t backslashAsCurrencySymbol;
};

static_assert(sizeof(TextEncoding) == 24);
static_assert(alignof(TextEncoding) == 8);
static_assert(offsetof(TextEncoding, vptr) == 0);
static_assert(offsetof(TextEncoding, name) == 8);
static_assert(offsetof(TextEncoding, backslashAsCurrencySymbol) == 16);
static_assert(sizeof(UnencodableHandling) == 1);

}  // namespace WebCore
