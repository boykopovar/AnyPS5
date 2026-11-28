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

#include <stdexcept>
#include <unicode/uchar.h>

namespace {

using NKWebKit::WtfAbi::StringView;

char16_t Latin1Unit(const StringView& string, unsigned index) {
    return string.is8Bit ? static_cast<const unsigned char*>(string.data)[index] : static_cast<const char16_t*>(string.data)[index];
}

UChar32 CodePointAt(const StringView& string, unsigned offset, unsigned& length) {
    const auto* characters = static_cast<const char16_t*>(string.data);
    const char16_t first = characters[offset];
    if (first >= 0xd800 && first <= 0xdbff && offset + 1 < string.length) {
        const char16_t second = characters[offset + 1];
        if (second >= 0xdc00 && second <= 0xdfff) {
            length = 2;
            return 0x10000 + ((first - 0xd800) << 10) + (second - 0xdc00);
        }
    }
    length = 1;
    return first;
}

UGraphemeClusterBreak GraphemeBreak(UChar32 character) {
    return static_cast<UGraphemeClusterBreak>(u_getIntPropertyValue(character, UCHAR_GRAPHEME_CLUSTER_BREAK));
}

UIndicConjunctBreak IndicConjunctBreak(UChar32 character) {
    return static_cast<UIndicConjunctBreak>(u_getIntPropertyValue(character, UCHAR_INDIC_CONJUNCT_BREAK));
}

struct GraphemeProperties {
    UGraphemeClusterBreak graphemeBreak;
    UIndicConjunctBreak indicConjunctBreak;
    bool extendedPictographic;
};

GraphemeProperties Properties(UChar32 character) {
    return {
        GraphemeBreak(character),
        IndicConjunctBreak(character),
        static_cast<bool>(u_hasBinaryProperty(character, UCHAR_EXTENDED_PICTOGRAPHIC))
    };
}

bool IsGraphemeControl(UGraphemeClusterBreak value) {
    return value == U_GCB_CONTROL || value == U_GCB_CR || value == U_GCB_LF;
}

struct GraphemeState {
    unsigned regionalIndicatorCount = 0;
    bool extendedPictographicExtendSuffix = false;
    bool previousZwjFollowsPictographic = false;
    bool indicConsonantSuffix = false;
    bool indicLinkerSuffix = false;

    void Add(const GraphemeProperties& character) {
        previousZwjFollowsPictographic =
            character.graphemeBreak == U_GCB_ZWJ && extendedPictographicExtendSuffix;
        if (character.extendedPictographic)
            extendedPictographicExtendSuffix = true;
        else if (character.graphemeBreak != U_GCB_EXTEND)
            extendedPictographicExtendSuffix = false;

        if (character.indicConjunctBreak == U_INCB_CONSONANT) {
            indicConsonantSuffix = true;
            indicLinkerSuffix = false;
        } else if (character.indicConjunctBreak == U_INCB_EXTEND ||
                   character.indicConjunctBreak == U_INCB_LINKER) {
            indicLinkerSuffix = indicLinkerSuffix || character.indicConjunctBreak == U_INCB_LINKER;
        } else {
            indicConsonantSuffix = false;
            indicLinkerSuffix = false;
        }

        if (character.graphemeBreak == U_GCB_REGIONAL_INDICATOR)
            ++regionalIndicatorCount;
        else
            regionalIndicatorCount = 0;
    }
};

bool ShouldBreak(const GraphemeProperties& previous, const GraphemeProperties& current, const GraphemeState& state) {
    const UGraphemeClusterBreak previousValue = previous.graphemeBreak;
    const UGraphemeClusterBreak currentValue = current.graphemeBreak;
    if (previousValue == U_GCB_CR && currentValue == U_GCB_LF) return false;
    if (IsGraphemeControl(previousValue) || IsGraphemeControl(currentValue)) return true;
    if (previousValue == U_GCB_L &&
        (currentValue == U_GCB_L || currentValue == U_GCB_V || currentValue == U_GCB_LV || currentValue == U_GCB_LVT))
        return false;
    if ((previousValue == U_GCB_LV || previousValue == U_GCB_V) &&
        (currentValue == U_GCB_V || currentValue == U_GCB_T))
        return false;
    if ((previousValue == U_GCB_LVT || previousValue == U_GCB_T) && currentValue == U_GCB_T) return false;
    if (currentValue == U_GCB_EXTEND || currentValue == U_GCB_ZWJ || currentValue == U_GCB_SPACING_MARK) return false;
    if (previousValue == U_GCB_PREPEND) return false;
    if (current.indicConjunctBreak == U_INCB_CONSONANT && state.indicConsonantSuffix && state.indicLinkerSuffix)
        return false;
    if (current.extendedPictographic && previousValue == U_GCB_ZWJ && state.previousZwjFollowsPictographic)
        return false;
    if (previousValue == U_GCB_REGIONAL_INDICATOR && currentValue == U_GCB_REGIONAL_INDICATOR) {
        if (state.regionalIndicatorCount % 2) return false;
    }
    return true;
}

unsigned CountGraphemeClusters(const StringView& string) {
    if (!string.length) return 0;
    unsigned clusters = 1;
    unsigned previousLength;
    GraphemeProperties previous = Properties(CodePointAt(string, 0, previousLength));
    GraphemeState state;
    state.Add(previous);
    for (unsigned offset = previousLength; offset < string.length;) {
        unsigned length;
        const GraphemeProperties current = Properties(CodePointAt(string, offset, length));
        if (ShouldBreak(previous, current, state)) ++clusters;
        previous = current;
        state.Add(current);
        offset += length;
    }
    return clusters;
}

unsigned CodeUnitsInGraphemeClusters(const StringView& string, unsigned clusters) {
    if (!clusters) return 0;
    unsigned count = 1;
    unsigned previousLength;
    GraphemeProperties previous = Properties(CodePointAt(string, 0, previousLength));
    GraphemeState state;
    state.Add(previous);
    for (unsigned offset = previousLength; offset < string.length;) {
        unsigned length;
        const GraphemeProperties current = Properties(CodePointAt(string, offset, length));
        if (ShouldBreak(previous, current, state)) {
            if (count == clusters) return offset;
            ++count;
        }
        previous = current;
        state.Add(current);
        offset += length;
    }
    return string.length;
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
    return CountGraphemeClusters(string);
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
    return CodeUnitsInGraphemeClusters(string, numGraphemeClusters);
}

}
