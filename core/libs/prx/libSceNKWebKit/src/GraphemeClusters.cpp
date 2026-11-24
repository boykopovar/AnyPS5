// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/text/TextBreakIterator.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "prx/libSceNKWebKit/include/GraphemeClusterExports.hpp"
#include "prx/libc/include/General.hpp"

#include <memory>
#include <stdexcept>
#include <unicode/brkiter.h>
#include <unicode/unistr.h>

namespace {

using NKWebKit::WtfAbi::StringView;

char16_t Latin1Unit(const StringView& string, unsigned index) {
    return string.is8Bit ? static_cast<const unsigned char*>(string.data)[index] : static_cast<const char16_t*>(string.data)[index];
}

std::unique_ptr<icu::BreakIterator> CharacterBreakIterator(const StringView& string) {
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::BreakIterator> iterator(
        icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), status));
    if (U_FAILURE(status) || !iterator)
        throw std::runtime_error("failed to create grapheme break iterator");
    if (string.is8Bit)
        throw std::invalid_argument("unexpected 8-bit grapheme input");
    if (!string.data)
        throw std::invalid_argument("null WTF character buffer");
    icu::UnicodeString text(static_cast<const char16_t*>(string.data), string.length);
    iterator->setText(text);
    return iterator;
}

}

extern "C" {

unsigned APS5_VABI _ZN3WTF19numGraphemeClustersENS_10StringViewE(StringView string) {
    unsigned stringLength = string.length;
    if (!stringLength) return 0;
    if (!string.data) throw std::invalid_argument("null WTF character buffer");
    if (string.is8Bit) {
        unsigned numCRLF = 0;
        for (unsigned i = 1; i < stringLength; ++i)
            numCRLF += Latin1Unit(string, i - 1) == '\r' && Latin1Unit(string, i) == '\n';
        return stringLength - numCRLF;
    }
    std::unique_ptr<icu::BreakIterator> iterator(CharacterBreakIterator(string));
    unsigned clusters = 0;
    for (int32_t boundary = iterator->first(); boundary != icu::BreakIterator::DONE; boundary = iterator->next())
        ++clusters;
    return clusters ? clusters - 1 : 0;
}

unsigned APS5_VABI _ZN3WTF30numCodeUnitsInGraphemeClustersENS_10StringViewEj(StringView string, unsigned numGraphemeClusters) {
    unsigned stringLength = string.length;
    if (stringLength <= numGraphemeClusters) return stringLength;
    if (!string.data) throw std::invalid_argument("null WTF character buffer");
    if (string.is8Bit) {
        unsigned i, j;
        for (i = 0, j = 0; i < numGraphemeClusters && j + 1 < stringLength; ++i, ++j)
            j += Latin1Unit(string, j) == '\r' && Latin1Unit(string, j + 1) == '\n';
        return j + (i < numGraphemeClusters);
    }
    std::unique_ptr<icu::BreakIterator> iterator(CharacterBreakIterator(string));
    int32_t boundary = iterator->first();
    for (unsigned i = 0; i < numGraphemeClusters && boundary != icu::BreakIterator::DONE; ++i)
        boundary = iterator->next();
    if (boundary == icu::BreakIterator::DONE) return stringLength;
    return static_cast<unsigned>(boundary);
}

}
