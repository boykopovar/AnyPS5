/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Tony Chang <idealisms@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/Api.hpp"
#include "prx/libSceIcu/Properties.hpp"
#include "prx/libSceIcu/Space.hpp"

#include <string_view>

extern "C" bool APS5_VABI _ZN7WebCore29isCharacterSmartReplaceExemptEib(int character, bool isPreviousCharacter) {
    if (u_isUWhiteSpace_nid_postfix(character)) return true;
    if ((character >= 0x1100 && character <= 0x1100 + 256)
        || (character >= 0x2E80 && character <= 0x2E80 + 352)
        || (character >= 0x2FF0 && character <= 0x2FF0 + 464)
        || (character >= 0x3200 && character <= 0x3200 + 29392)
        || (character >= 0xAC00 && character <= 0xAC00 + 11183)
        || (character >= 0xF900 && character <= 0xF900 + 352)
        || (character >= 0xFE30 && character <= 0xFE30 + 32)
        || (character >= 0xFF00 && character <= 0xFF00 + 240)
        || (character >= 0x20000 && character <= 0x20000 + 0xA6D7)
        || (character >= 0x2F800 && character <= 0x2F800 + 0x021E)) return true;

    const std::u32string_view punctuation = isPreviousCharacter ? U"([\"'#$/-`{" : U")].,;:?'!\"%*-/}";
    if (punctuation.find(static_cast<char32_t>(character)) != std::u32string_view::npos) return true;
    if (isPreviousCharacter) return false;
    const auto category = u_charType_nid_postfix(character);
    return (category >= 19 && category <= 23) || category == 28 || category == 29;
}
