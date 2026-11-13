// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebCore/svg/SVGTests.cpp
/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2015-2021 Apple Inc. All right reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/src/WtfLettersLiteral.hpp"

#include <string_view>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using namespace std::string_view_literals;

constexpr std::string_view SupportedSVGFeatures[] = {
    "http://www.w3.org/tr/svg11/feature#animation"sv,
    "http://www.w3.org/tr/svg11/feature#basegraphicsattribute"sv,
    "http://www.w3.org/tr/svg11/feature#basicclip"sv,
    "http://www.w3.org/tr/svg11/feature#basicfilter"sv,
    "http://www.w3.org/tr/svg11/feature#basicfont"sv,
    "http://www.w3.org/tr/svg11/feature#basicpaintattribute"sv,
    "http://www.w3.org/tr/svg11/feature#basicstructure"sv,
    "http://www.w3.org/tr/svg11/feature#basictext"sv,
    "http://www.w3.org/tr/svg11/feature#clip"sv,
    "http://www.w3.org/tr/svg11/feature#conditionalprocessing"sv,
    "http://www.w3.org/tr/svg11/feature#containerattribute"sv,
    "http://www.w3.org/tr/svg11/feature#coreattribute"sv,
    "http://www.w3.org/tr/svg11/feature#cursor"sv,
    "http://www.w3.org/tr/svg11/feature#documenteventsattribute"sv,
    "http://www.w3.org/tr/svg11/feature#extensibility"sv,
    "http://www.w3.org/tr/svg11/feature#externalresourcesrequired"sv,
    "http://www.w3.org/tr/svg11/feature#filter"sv,
    "http://www.w3.org/tr/svg11/feature#font"sv,
    "http://www.w3.org/tr/svg11/feature#gradient"sv,
    "http://www.w3.org/tr/svg11/feature#graphicaleventsattribute"sv,
    "http://www.w3.org/tr/svg11/feature#graphicsattribute"sv,
    "http://www.w3.org/tr/svg11/feature#hyperlinking"sv,
    "http://www.w3.org/tr/svg11/feature#image"sv,
    "http://www.w3.org/tr/svg11/feature#marker"sv,
    "http://www.w3.org/tr/svg11/feature#mask"sv,
    "http://www.w3.org/tr/svg11/feature#opacityattribute"sv,
    "http://www.w3.org/tr/svg11/feature#paintattribute"sv,
    "http://www.w3.org/tr/svg11/feature#pattern"sv,
    "http://www.w3.org/tr/svg11/feature#script"sv,
    "http://www.w3.org/tr/svg11/feature#shape"sv,
    "http://www.w3.org/tr/svg11/feature#structure"sv,
    "http://www.w3.org/tr/svg11/feature#style"sv,
    "http://www.w3.org/tr/svg11/feature#svg"sv,
    "http://www.w3.org/tr/svg11/feature#svg-animation"sv,
    "http://www.w3.org/tr/svg11/feature#svg-static"sv,
    "http://www.w3.org/tr/svg11/feature#svgdom"sv,
    "http://www.w3.org/tr/svg11/feature#svgdom-animation"sv,
    "http://www.w3.org/tr/svg11/feature#svgdom-static"sv,
    "http://www.w3.org/tr/svg11/feature#text"sv,
    "http://www.w3.org/tr/svg11/feature#view"sv,
    "http://www.w3.org/tr/svg11/feature#viewportattribute"sv,
    "http://www.w3.org/tr/svg11/feature#xlinkattribute"sv,
    "org.w3c.dom"sv,
    "org.w3c.dom.svg"sv,
    "org.w3c.dom.svg.static"sv,
    "org.w3c.svg"sv,
    "org.w3c.svg.static"sv,
};

bool EqualsLiteral(const StringView& string, std::string_view literal) {
    if (string.length != literal.size()) return false;
    for (unsigned i = 0; i < literal.size(); ++i) {
        if (NKWebKit::WtfAbi::CodeUnitAt(string, i) != static_cast<unsigned char>(literal[i])) return false;
    }
    return true;
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCore8SVGTests27hasFeatureForLegacyBindingsERKN3WTF6StringES4_(const String& feature, const String& version) {
    StringView featureView = NKWebKit::WtfAbi::ViewOf(feature.impl);
    StringView versionView = NKWebKit::WtfAbi::ViewOf(version.impl);
    bool hasSVG10FeaturePrefix = NKWebKit::WtfAbi::StartsWithLettersIgnoringASCIICase(featureView, "org.w3c.dom.svg"sv)
        || NKWebKit::WtfAbi::StartsWithLettersIgnoringASCIICase(featureView, "org.w3c.svg"sv);
    bool hasSVG11FeaturePrefix = NKWebKit::WtfAbi::StartsWithLettersIgnoringASCIICase(featureView, "http://www.w3.org/tr/svg"sv);
    if (!(hasSVG10FeaturePrefix || hasSVG11FeaturePrefix)) return true;
    if (!versionView.length || (hasSVG10FeaturePrefix && EqualsLiteral(versionView, "1.0"sv))
        || (hasSVG11FeaturePrefix && EqualsLiteral(versionView, "1.1"sv))) {
        return NKWebKit::WtfAbi::FindLettersIgnoringASCIICase(SupportedSVGFeatures, [](std::string_view entry) { return entry; }, featureView) != nullptr;
    }
    return false;
}

}
