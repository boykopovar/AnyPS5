/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
 *
 * Derived from Source/WebCore/dom/ViewportArguments.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 * setViewportFeature follows Source/WebCore/dom/ViewportArguments.cpp@e12a212a531aeddc11259e8515671cf5188e038d
 */

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace WebCore {

namespace {

const float& CompareIgnoringAuto(const float& value1, const float& value2, const float& (*compare)(const float&, const float&)) {
    if (value1 == ViewportArguments::ValueAuto) {
        return value2;
    }
    if (value2 == ViewportArguments::ValueAuto) {
        return value1;
    }
    return compare(value1, value2);
}

const float& MinFloat(const float& a, const float& b) {
    return std::min(a, b);
}

const float& MaxFloat(const float& a, const float& b) {
    return std::max(a, b);
}

float ClampLengthValue(float value) {
    if (value != ViewportArguments::ValueAuto) {
        return std::min<float>(10000, std::max<float>(value, 1));
    }
    return value;
}

float ClampScaleValue(float value) {
    if (value != ViewportArguments::ValueAuto) {
        return std::min<float>(10, std::max<float>(value, 0.1));
    }
    return value;
}

FloatSize ConvertToUserSpace(const FloatSize& deviceSize, float devicePixelRatio) {
    FloatSize result = deviceSize;
    if (devicePixelRatio != 1) {
        float s = 1 / devicePixelRatio;
        result.m_width *= s;
        result.m_height *= s;
    }
    return result;
}

FloatSize ToFloatSize(const IntSize& size) {
    return FloatSize{static_cast<float>(size.m_width), static_cast<float>(size.m_height)};
}

float ResolveDeviceKeyword(float value, const FloatSize& deviceSize) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) {
        return value;
    }
    switch (static_cast<int>(value)) {
    case ViewportArguments::ValueDeviceWidth:
        return deviceSize.m_width;
    case ViewportArguments::ValueDeviceHeight:
        return deviceSize.m_height;
    }
    return value;
}

ViewportAttributes Resolve(const ViewportArguments& args, const FloatSize& initialViewportSize, const FloatSize& deviceSize, int defaultWidth) {
    float resultWidth = ResolveDeviceKeyword(args.width, deviceSize);
    float resultMaxWidth = args.maxWidth;
    float resultMinWidth = args.minWidth;
    float resultHeight = ResolveDeviceKeyword(args.height, deviceSize);
    float resultMinHeight = args.minHeight;
    float resultMaxHeight = args.maxHeight;
    float resultZoom = args.zoom;
    float resultMinZoom = args.minZoom;
    float resultMaxZoom = args.maxZoom;

    if (args.type == ViewportArguments::CSSDeviceAdaptation) {
        resultMinWidth = ResolveDeviceKeyword(resultMinWidth, deviceSize);
        resultMaxWidth = ResolveDeviceKeyword(resultMaxWidth, deviceSize);
        resultMinHeight = ResolveDeviceKeyword(resultMinHeight, deviceSize);
        resultMaxHeight = ResolveDeviceKeyword(resultMaxHeight, deviceSize);

        if (resultMinWidth != ViewportArguments::ValueAuto || resultMaxWidth != ViewportArguments::ValueAuto) {
            resultWidth = CompareIgnoringAuto(resultMinWidth, CompareIgnoringAuto(resultMaxWidth, deviceSize.m_width, MinFloat), MaxFloat);
        }

        if (resultMinHeight != ViewportArguments::ValueAuto || resultMaxHeight != ViewportArguments::ValueAuto) {
            resultHeight = CompareIgnoringAuto(resultMinHeight, CompareIgnoringAuto(resultMaxHeight, deviceSize.m_height, MinFloat), MaxFloat);
        }

        if (resultMinZoom != ViewportArguments::ValueAuto && resultMaxZoom != ViewportArguments::ValueAuto) {
            resultMaxZoom = std::max(resultMinZoom, resultMaxZoom);
        }

        if (resultZoom != ViewportArguments::ValueAuto) {
            resultZoom = CompareIgnoringAuto(resultMinZoom, CompareIgnoringAuto(resultMaxZoom, resultZoom, MinFloat), MaxFloat);
        }

        if (resultWidth == ViewportArguments::ValueAuto && resultZoom == ViewportArguments::ValueAuto) {
            resultWidth = deviceSize.m_width;
        }

        if (resultWidth == ViewportArguments::ValueAuto && resultHeight == ViewportArguments::ValueAuto) {
            resultWidth = deviceSize.m_width / resultZoom;
        }

        if (resultWidth == ViewportArguments::ValueAuto) {
            resultWidth = resultHeight * deviceSize.m_width / deviceSize.m_height;
        }

        if (resultHeight == ViewportArguments::ValueAuto) {
            resultHeight = resultWidth * deviceSize.m_height / deviceSize.m_width;
        }

        if (resultZoom != ViewportArguments::ValueAuto || resultMaxZoom != ViewportArguments::ValueAuto) {
            resultWidth = CompareIgnoringAuto(resultWidth, deviceSize.m_width / CompareIgnoringAuto(resultZoom, resultMaxZoom, MinFloat), MaxFloat);
            resultHeight = CompareIgnoringAuto(resultHeight, deviceSize.m_height / CompareIgnoringAuto(resultZoom, resultMaxZoom, MinFloat), MaxFloat);
        }

        resultWidth = std::max<float>(1, resultWidth);
        resultHeight = std::max<float>(1, resultHeight);
    }

    if (args.type != ViewportArguments::CSSDeviceAdaptation && args.type != ViewportArguments::Implicit) {
        resultWidth = ClampLengthValue(resultWidth);
        resultHeight = ClampLengthValue(resultHeight);
        resultZoom = ClampScaleValue(resultZoom);
        resultMinZoom = ClampScaleValue(resultMinZoom);
        resultMaxZoom = ClampScaleValue(resultMaxZoom);
    }

    ViewportAttributes result{};

    if (resultMinZoom == ViewportArguments::ValueAuto) {
        result.minimumScale = 0.25f;
    } else {
        result.minimumScale = resultMinZoom;
    }

    if (resultMaxZoom == ViewportArguments::ValueAuto) {
        result.maximumScale = 5;
        result.minimumScale = std::min<float>(5, result.minimumScale);
    } else {
        result.maximumScale = resultMaxZoom;
    }
    result.maximumScale = std::max(result.minimumScale, result.maximumScale);

    result.initialScale = resultZoom;
    if (resultZoom == ViewportArguments::ValueAuto) {
        result.initialScale = initialViewportSize.m_width / static_cast<float>(defaultWidth);
        if (resultWidth != ViewportArguments::ValueAuto) {
            result.initialScale = initialViewportSize.m_width / resultWidth;
        }
        if (resultHeight != ViewportArguments::ValueAuto) {
            result.initialScale = std::max<float>(result.initialScale, initialViewportSize.m_height / resultHeight);
        }
    }

    result.initialScale = std::min(result.maximumScale, std::max(result.minimumScale, result.initialScale));

    if (resultWidth == ViewportArguments::ValueAuto) {
        if (resultZoom == ViewportArguments::ValueAuto) {
            resultWidth = static_cast<float>(defaultWidth);
        } else if (resultHeight != ViewportArguments::ValueAuto) {
            resultWidth = resultHeight * (initialViewportSize.m_width / initialViewportSize.m_height);
        } else {
            resultWidth = initialViewportSize.m_width / result.initialScale;
        }
    }

    if (resultHeight == ViewportArguments::ValueAuto) {
        resultHeight = resultWidth * (initialViewportSize.m_height / initialViewportSize.m_width);
    }

    if (args.type == ViewportArguments::ViewportMeta) {
        resultWidth = std::max<float>(resultWidth, initialViewportSize.m_width / result.initialScale);
        resultHeight = std::max<float>(resultHeight, initialViewportSize.m_height / result.initialScale);
    }

    result.layoutSize.m_width = resultWidth;
    result.layoutSize.m_height = resultHeight;

    result.userScalable = args.userZoom;
    result.orientation = args.orientation;
    result.shrinkToFit = args.shrinkToFit;
    result.viewportFit = args.viewportFit;

    return result;
}

using NKWebKit::WtfAbi::StringView;

bool EqualLettersIgnoringASCIICase(StringView view, const char* letters) {
    if (view.length != std::strlen(letters)) {
        return false;
    }
    for (unsigned i = 0; i < view.length; ++i) {
        char16_t c = NKWebKit::WtfAbi::CodeUnitAt(view, i);
        if (c >= u'A' && c <= u'Z') {
            c += u'a' - u'A';
        }
        if (c != static_cast<unsigned char>(letters[i])) {
            return false;
        }
    }
    return true;
}

std::u16string ToU16(StringView view) {
    std::u16string result;
    for (unsigned i = 0; i < view.length; ++i) {
        result += NKWebKit::WtfAbi::CodeUnitAt(view, i);
    }
    return result;
}

void ReplaceAll(std::u16string& text, const std::u16string& pattern, const std::u16string& replacement) {
    for (std::size_t at = text.find(pattern); at != std::u16string::npos; at = text.find(pattern, at + replacement.size())) {
        text.replace(at, pattern.size(), replacement);
    }
}

NKWebKit::WtfAbi::String MakeString(const std::u16string& text) {
    bool is8Bit = std::all_of(text.begin(), text.end(), [](char16_t c) { return c <= 0xff; });
    void* characters = nullptr;
    NKWebKit::WtfAbi::String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(text.size()), is8Bit, characters));
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (is8Bit) {
            static_cast<unsigned char*>(characters)[i] = static_cast<unsigned char>(text[i]);
        } else {
            static_cast<char16_t*>(characters)[i] = text[i];
        }
    }
    return result;
}

void ReportViewportError(const ViewportErrorHandler& errorHandler, ViewportErrorCode errorCode, StringView replacement1, StringView replacement2) {
    static constexpr const char* errors[] = {
        "Viewport argument key \"%replacement1\" not recognized and ignored.",
        "Viewport argument value \"%replacement1\" for key \"%replacement2\" is invalid, and has been ignored.",
        "Viewport argument value \"%replacement1\" for key \"%replacement2\" was truncated to its numeric prefix.",
        "Viewport maximum-scale cannot be larger than 10.0. The maximum-scale will be set to 10.0."
    };
    if (!errorHandler.callableWrapper) {
        return;
    }
    std::u16string message(errors[errorCode], errors[errorCode] + std::strlen(errors[errorCode]));
    if (replacement1.data) {
        ReplaceAll(message, u"%replacement1", ToU16(replacement1));
    }
    if (replacement2.data) {
        ReplaceAll(message, u"%replacement2", ToU16(replacement2));
    }
    if ((errorCode == UnrecognizedViewportArgumentValueError || errorCode == TruncatedViewportArgumentValueError) && ToU16(replacement1).find(u';') != std::u16string::npos) {
        message += u" Note that ';' is not a separator in viewport values. The list should be comma-separated.";
    }
    NKWebKit::WtfAbi::String text = MakeString(message);
    errorHandler.callableWrapper->vtable->call(errorHandler.callableWrapper, errorCode, text);
}

float NumericPrefix(StringView key, StringView value, const ViewportErrorHandler& errorHandler) {
    std::size_t parsedLength = 0;
    float numericValue = value.is8Bit
        ? _ZN3WTF17charactersToFloatEPKhmRm(static_cast<const unsigned char*>(value.data), value.length, &parsedLength)
        : _ZN3WTF17charactersToFloatEPKDsmRm(static_cast<const char16_t*>(value.data), value.length, &parsedLength);
    if (!parsedLength) {
        ReportViewportError(errorHandler, UnrecognizedViewportArgumentValueError, value, key);
        return 0;
    }
    if (parsedLength < value.length) {
        ReportViewportError(errorHandler, TruncatedViewportArgumentValueError, value, key);
    }
    return numericValue;
}

float FindSizeValue(StringView key, StringView value, const ViewportErrorHandler& errorHandler, bool* valueWasExplicit = nullptr) {
    if (valueWasExplicit) {
        *valueWasExplicit = true;
    }
    if (EqualLettersIgnoringASCIICase(value, "device-width")) {
        return ViewportArguments::ValueDeviceWidth;
    }
    if (EqualLettersIgnoringASCIICase(value, "device-height")) {
        return ViewportArguments::ValueDeviceHeight;
    }
    float sizeValue = NumericPrefix(key, value, errorHandler);
    if (sizeValue < 0) {
        if (valueWasExplicit) {
            *valueWasExplicit = false;
        }
        return ViewportArguments::ValueAuto;
    }
    return sizeValue;
}

float FindScaleValue(StringView key, StringView value, const ViewportErrorHandler& errorHandler) {
    if (EqualLettersIgnoringASCIICase(value, "yes")) {
        return 1;
    }
    if (EqualLettersIgnoringASCIICase(value, "no")) {
        return 0;
    }
    if (EqualLettersIgnoringASCIICase(value, "device-width") || EqualLettersIgnoringASCIICase(value, "device-height")) {
        return 10;
    }
    float numericValue = NumericPrefix(key, value, errorHandler);
    if (numericValue < 0) {
        return ViewportArguments::ValueAuto;
    }
    if (numericValue > 10.0) {
        ReportViewportError(errorHandler, MaximumScaleTooLargeError, {}, {});
    }
    return numericValue;
}

bool FindBooleanValue(StringView key, StringView value, const ViewportErrorHandler& errorHandler) {
    if (EqualLettersIgnoringASCIICase(value, "yes")) {
        return true;
    }
    if (EqualLettersIgnoringASCIICase(value, "no")) {
        return false;
    }
    if (EqualLettersIgnoringASCIICase(value, "device-width") || EqualLettersIgnoringASCIICase(value, "device-height")) {
        return true;
    }
    return std::abs(NumericPrefix(key, value, errorHandler)) >= 1;
}

ViewportFit ParseViewportFitValue(StringView key, StringView value, const ViewportErrorHandler& errorHandler) {
    if (EqualLettersIgnoringASCIICase(value, "auto")) {
        return ViewportFit::Auto;
    }
    if (EqualLettersIgnoringASCIICase(value, "contain")) {
        return ViewportFit::Contain;
    }
    if (EqualLettersIgnoringASCIICase(value, "cover")) {
        return ViewportFit::Cover;
    }
    ReportViewportError(errorHandler, UnrecognizedViewportArgumentValueError, value, key);
    return ViewportFit::Auto;
}

}

}

extern "C" {

WebCore::ViewportAttributes APS5_VABI _ZN7WebCore25computeViewportAttributesENS_17ViewportArgumentsEiiifNS_7IntSizeE(
    WebCore::ViewportArguments args, int desktopWidth, int deviceWidth, int deviceHeight, float devicePixelRatio, WebCore::IntSize visibleViewport) {
    WebCore::FloatSize initialViewportSize = WebCore::ConvertToUserSpace(WebCore::ToFloatSize(visibleViewport), devicePixelRatio);
    WebCore::FloatSize deviceSize = WebCore::ConvertToUserSpace(
        WebCore::FloatSize{static_cast<float>(deviceWidth), static_cast<float>(deviceHeight)}, devicePixelRatio);
    return WebCore::Resolve(args, initialViewportSize, deviceSize, desktopWidth);
}

float APS5_VABI _ZN7WebCore44computeMinimumScaleFactorForContentContainedERKNS_18ViewportAttributesERKNS_7IntSizeES5_(
    const WebCore::ViewportAttributes& result, const WebCore::IntSize& visibleViewport, const WebCore::IntSize& contentsSize) {
    WebCore::FloatSize viewportSize = WebCore::ToFloatSize(visibleViewport);
    return std::max<float>(result.minimumScale,
        std::max(viewportSize.m_width / static_cast<float>(contentsSize.m_width), viewportSize.m_height / static_cast<float>(contentsSize.m_height)));
}

void APS5_VABI _ZN7WebCore40restrictMinimumScaleFactorToViewportSizeERNS_18ViewportAttributesENS_7IntSizeEf(
    WebCore::ViewportAttributes& result, WebCore::IntSize visibleViewport, float devicePixelRatio) {
    WebCore::FloatSize viewportSize = WebCore::ConvertToUserSpace(WebCore::ToFloatSize(visibleViewport), devicePixelRatio);
    result.minimumScale = std::max<float>(result.minimumScale,
        std::max(viewportSize.m_width / result.layoutSize.m_width, viewportSize.m_height / result.layoutSize.m_height));
}

void APS5_VABI _ZN7WebCore50restrictScaleFactorToInitialScaleIfNotUserScalableERNS_18ViewportAttributesE(WebCore::ViewportAttributes& result) {
    if (!result.userScalable) {
        result.maximumScale = result.minimumScale = result.initialScale;
    }
}

void APS5_VABI _ZN7WebCore18setViewportFeatureERNS_17ViewportArgumentsEN3WTF10StringViewES3_bRKNS2_8FunctionIFvNS_17ViewportErrorCodeERKNS2_6StringEEEE(
    WebCore::ViewportArguments& arguments, NKWebKit::WtfAbi::StringView key, NKWebKit::WtfAbi::StringView value, bool viewportFitEnabled,
    const WebCore::ViewportErrorHandler& errorHandler) {
    using namespace WebCore;
    if (EqualLettersIgnoringASCIICase(key, "width")) {
        arguments.width = FindSizeValue(key, value, errorHandler, &arguments.widthWasExplicit);
    } else if (EqualLettersIgnoringASCIICase(key, "height")) {
        arguments.height = FindSizeValue(key, value, errorHandler);
    } else if (EqualLettersIgnoringASCIICase(key, "initial-scale")) {
        arguments.zoom = FindScaleValue(key, value, errorHandler);
    } else if (EqualLettersIgnoringASCIICase(key, "minimum-scale")) {
        arguments.minZoom = FindScaleValue(key, value, errorHandler);
    } else if (EqualLettersIgnoringASCIICase(key, "maximum-scale")) {
        arguments.maxZoom = FindScaleValue(key, value, errorHandler);
    } else if (EqualLettersIgnoringASCIICase(key, "user-scalable")) {
        arguments.userZoom = FindBooleanValue(key, value, errorHandler);
    } else if (EqualLettersIgnoringASCIICase(key, "shrink-to-fit")) {
        arguments.shrinkToFit = FindBooleanValue(key, value, errorHandler);
    } else if (EqualLettersIgnoringASCIICase(key, "viewport-fit") && viewportFitEnabled) {
        arguments.viewportFit = ParseViewportFitValue(key, value, errorHandler);
    } else {
        ReportViewportError(errorHandler, UnrecognizedViewportArgumentKeyError, key, {});
    }
}

}
