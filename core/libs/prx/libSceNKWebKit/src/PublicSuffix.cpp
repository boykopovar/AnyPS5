/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/network/curl/PublicSuffixCurl.cpp
 */

#include "prx/libSceNKWebKit/include/PublicSuffixExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <libpsl.h>
#include <stdexcept>

extern "C" bool APS5_VABI _ZN7WebCore14isPublicSuffixERKN3WTF6StringE(const NKWebKit::WtfAbi::String* domain) {
    if (!domain) throw std::invalid_argument("WebCore::isPublicSuffix: null string");
    if (!domain->impl || !domain->impl->length) return false;
    const psl_ctx_t* suffixes = psl_builtin();
    if (!suffixes) throw std::runtime_error("WebCore::isPublicSuffix: no built-in suffix list");
    const auto lowercase = _ZNK3WTF6String31convertToLowercaseWithoutLocaleEv(domain);
    const auto utf8 = _ZNK3WTF6String4utf8ENS_14ConversionModeE(&lowercase, 0);
    if (!utf8.buffer) throw std::runtime_error("WebCore::isPublicSuffix: UTF-8 conversion failed");
    return psl_is_public_suffix2(suffixes, reinterpret_cast<const char*>(utf8.buffer + 1),
        PSL_TYPE_ANY | PSL_TYPE_NO_STAR_RULE);
}
