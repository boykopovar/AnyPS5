// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/filters/FilterOperation.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/FilterOperationExports.hpp"
#include "prx/libSceNKWebKit/include/Api.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/NoOpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace {

struct IntOutsets {
    std::int32_t sides[4];
};

struct ColorComponents {
    float components[4];

    float& operator[](std::size_t index) { return components[index]; }
    const float& operator[](std::size_t index) const { return components[index]; }
};

struct FilterOperationVtable {
    void (APS5_VABI* completeDestructor)(void*);
    void (APS5_VABI* deletingDestructor)(void*);
    WTF::Ref<WebCore::FilterOperation> (APS5_VABI* clone)(const void*);
    bool (APS5_VABI* equals)(const void*, const WebCore::FilterOperation&);
    WTF::RefPtr<WebCore::FilterOperation> (APS5_VABI* blend)(void*, const WebCore::FilterOperation*, double, bool);
    bool (APS5_VABI* transformColor)(const void*, WebCore::SRGBA<float>&);
    bool (APS5_VABI* inverseTransformColor)(const void*, WebCore::SRGBA<float>&);
    bool (APS5_VABI* isIdentity)(const void*);
    IntOutsets (APS5_VABI* outsets)(const void*);
    bool (APS5_VABI* affectsOpacity)(const void*);
    bool (APS5_VABI* movesPixels)(const void*);
    bool (APS5_VABI* shouldBeRestrictedBySecurityOrigin)(const void*);
};

struct FilterOperationVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    FilterOperationVtable dispatch;
};

static_assert(!std::is_trivially_destructible_v<WTF::Ref<WebCore::FilterOperation>>);

static_assert(offsetof(WebCore::FilterOperation, m_vptr) == 0);
static_assert(offsetof(WebCore::FilterOperation, m_refCount) == 8);
static_assert(offsetof(WebCore::FilterOperation, m_type) == 12);
static_assert(sizeof(WebCore::FilterOperation) == 16);
static_assert(sizeof(WebCore::DefaultFilterOperation) == 24);
static_assert(sizeof(WebCore::InvertLightnessFilterOperation) == 16);
static_assert(sizeof(WTF::Ref<WebCore::DefaultFilterOperation>) == 8);
static_assert(sizeof(WTF::RefPtr<WebCore::FilterOperation>) == 8);
static_assert(!std::is_trivially_destructible_v<WTF::Ref<WebCore::DefaultFilterOperation>>);
static_assert(!std::is_trivially_destructible_v<WTF::RefPtr<WebCore::FilterOperation>>);

WTF::RefPtr<WebCore::FilterOperation> APS5_VABI BaseBlend(void*, const WebCore::FilterOperation*, double, bool) {
    return nullptr;
}

bool APS5_VABI BaseTransformColor(const void*, WebCore::SRGBA<float>&) {
    return false;
}

bool APS5_VABI BaseInverseTransformColor(const void*, WebCore::SRGBA<float>&) {
    return false;
}

bool APS5_VABI BaseIsIdentity(const void*) {
    return false;
}

IntOutsets APS5_VABI BaseOutsets(const void*) {
    return {};
}

bool APS5_VABI BaseAffectsOpacity(const void*) {
    return false;
}

bool APS5_VABI BaseMovesPixels(const void*) {
    return false;
}

bool APS5_VABI BaseShouldBeRestrictedBySecurityOrigin(const void*) {
    return false;
}

const FilterOperationVtableStorage DefaultFilterOperationVtableInstance = {
    0,
    nullptr,
    {
        reinterpret_cast<void (APS5_VABI*)(void*)>(&_ZN7WebCore22DefaultFilterOperationD1Ev),
        reinterpret_cast<void (APS5_VABI*)(void*)>(&_ZN7WebCore22DefaultFilterOperationD0Ev),
        reinterpret_cast<WTF::Ref<WebCore::FilterOperation> (APS5_VABI*)(const void*)>(&_ZNK7WebCore22DefaultFilterOperation5cloneEv),
        reinterpret_cast<bool (APS5_VABI*)(const void*, const WebCore::FilterOperation&)>(&_ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE),
        &BaseBlend,
        &BaseTransformColor,
        &BaseInverseTransformColor,
        &BaseIsIdentity,
        &BaseOutsets,
        &BaseAffectsOpacity,
        &BaseMovesPixels,
        &BaseShouldBeRestrictedBySecurityOrigin,
    },
};

const FilterOperationVtableStorage InvertLightnessFilterOperationVtableInstance = {
    0,
    nullptr,
    {
        reinterpret_cast<void (APS5_VABI*)(void*)>(&_ZN7WebCore30InvertLightnessFilterOperationD1Ev),
        reinterpret_cast<void (APS5_VABI*)(void*)>(&_ZN7WebCore30InvertLightnessFilterOperationD0Ev),
        reinterpret_cast<WTF::Ref<WebCore::FilterOperation> (APS5_VABI*)(const void*)>(&_ZNK7WebCore30InvertLightnessFilterOperation5cloneEv),
        reinterpret_cast<bool (APS5_VABI*)(const void*, const WebCore::FilterOperation&)>(&_ZNK7WebCore30InvertLightnessFilterOperationeqERKNS_15FilterOperationE),
        reinterpret_cast<WTF::RefPtr<WebCore::FilterOperation> (APS5_VABI*)(void*, const WebCore::FilterOperation*, double, bool)>(&_ZN7WebCore30InvertLightnessFilterOperation5blendEPKNS_15FilterOperationEdb),
        reinterpret_cast<bool (APS5_VABI*)(const void*, WebCore::SRGBA<float>&)>(&_ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE),
        reinterpret_cast<bool (APS5_VABI*)(const void*, WebCore::SRGBA<float>&)>(&_ZNK7WebCore30InvertLightnessFilterOperation21inverseTransformColorERNS_5SRGBAIfEE),
        &BaseIsIdentity,
        &BaseOutsets,
        &BaseAffectsOpacity,
        &BaseMovesPixels,
        &BaseShouldBeRestrictedBySecurityOrigin,
    },
};

WebCore::DefaultFilterOperation* Checked(WebCore::DefaultFilterOperation* self) {
    if (!self) throw std::invalid_argument("WebCore::DefaultFilterOperation: null this");
    return self;
}

const WebCore::InvertLightnessFilterOperation* Checked(const WebCore::InvertLightnessFilterOperation* self) {
    if (!self) throw std::invalid_argument("WebCore::InvertLightnessFilterOperation: null this");
    return self;
}

void ConstructDefaultFilterOperation(WebCore::DefaultFilterOperation* self, std::uint32_t representedType) {
    std::memset(self, 0, sizeof(WebCore::DefaultFilterOperation));
    self->m_vptr = const_cast<FilterOperationVtable*>(&DefaultFilterOperationVtableInstance.dispatch);
    self->m_refCount.store(1, std::memory_order_relaxed);
    self->m_type = WebCore::FilterOperationType::Default;
    *reinterpret_cast<std::uint32_t*>(&self->m_representedType) = representedType;
}

void ConstructInvertLightnessFilterOperation(WebCore::InvertLightnessFilterOperation* self) {
    std::memset(self, 0, sizeof(WebCore::InvertLightnessFilterOperation));
    self->m_vptr = const_cast<FilterOperationVtable*>(&InvertLightnessFilterOperationVtableInstance.dispatch);
    self->m_refCount.store(1, std::memory_order_relaxed);
    self->m_type = WebCore::FilterOperationType::AppleInvertLightness;
}

ColorComponents ResolvedComponents(const WebCore::SRGBA<float>& color) {
    return {std::isnan(color.red) ? 0.0f : color.red, std::isnan(color.green) ? 0.0f : color.green,
            std::isnan(color.blue) ? 0.0f : color.blue, std::isnan(color.alpha) ? 0.0f : color.alpha};
}

ColorComponents HueRotate(const ColorComponents& color, float amount) {
    float r = color[0];
    float g = color[1];
    float b = color[2];
    float alpha = color[3];

    auto [min, max] = std::minmax({r, g, b});
    float chroma = max - min;

    float lightness = 0.5f * (max + min);
    float saturation;
    if (!chroma)
        saturation = 0;
    else if (lightness <= 0.5f)
        saturation = (chroma / (max + min));
    else
        saturation = (chroma / (2.0f - (max + min)));

    if (!saturation)
        return {lightness, lightness, lightness, alpha};

    float hue;
    if (!chroma)
        hue = 0;
    else if (max == r)
        hue = (60.0f * ((g - b) / chroma)) + 360.0f;
    else if (max == g)
        hue = (60.0f * ((b - r) / chroma)) + 120.0f;
    else
        hue = (60.0f * ((r - g) / chroma)) + 240.0f;

    if (hue >= 360.0f)
        hue -= 360.0f;

    hue /= 360.0f;

    hue = std::fmod(hue + amount, 1.0f);

    float temp2 = lightness <= 0.5f ? lightness * (1.0f + saturation) : lightness + saturation - lightness * saturation;
    float temp1 = 2.0f * lightness - temp2;

    hue *= 6.0f;

    auto calcHue = [](float temp1, float temp2, float hueVal) {
        if (hueVal < 0.0f)
            hueVal += 6.0f;
        else if (hueVal >= 6.0f)
            hueVal -= 6.0f;
        if (hueVal < 1.0f)
            return temp1 + (temp2 - temp1) * hueVal;
        if (hueVal < 3.0f)
            return temp2;
        if (hueVal < 4.0f)
            return temp1 + (temp2 - temp1) * (4.0f - hueVal);
        return temp1;
    };

    return {calcHue(temp1, temp2, hue + 2.0f), calcHue(temp1, temp2, hue), calcHue(temp1, temp2, hue - 2.0f), alpha};
}

ColorComponents TransformColorComponents(const float (&matrix)[15], const ColorComponents& input) {
    ColorComponents result{0.0f, 0.0f, 0.0f, 0.0f};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column)
            result[row] += matrix[row * 5 + column] * input[column];
        result[row] += matrix[row * 5 + 4];
    }
    result[3] = input[3];
    return result;
}

float ClampedComponent(float component) {
    return std::clamp(component, 0.0f, 1.0f);
}

WebCore::SRGBA<float> ClampedColor(const ColorComponents& components) {
    return {ClampedComponent(components[0]), ClampedComponent(components[1]), ClampedComponent(components[2]),
            ClampedComponent(components[3])};
}

constexpr float ToDarkModeMatrix[15] = {
    -0.770f, 0.059f,  -0.089f, 0.0f, 1.0f,
    0.030f,  -0.741f, -0.089f, 0.0f, 1.0f,
    0.030f,  0.059f,  -0.890f, 0.0f, 1.0f
};

constexpr float ToLightModeMatrix[15] = {
    -1.300f, -0.097f, 0.147f,  0.0f, 1.25f,
    -0.049f, -1.347f, 0.146f,  0.0f, 1.25f,
    -0.049f, -0.097f, -1.104f, 0.0f, 1.25f
};

}

extern "C" {

WTF::Ref<WebCore::DefaultFilterOperation> APS5_VABI _ZN7WebCore22DefaultFilterOperation6createENS_15FilterOperation13OperationTypeE(std::uint32_t representedType) {
    auto* operation = static_cast<WebCore::DefaultFilterOperation*>(_ZN3WTF10fastMallocEm(sizeof(WebCore::DefaultFilterOperation)));
    ConstructDefaultFilterOperation(operation, representedType);
    return WTF::adoptRef(*operation);
}

void APS5_VABI _ZN7WebCore22DefaultFilterOperationC1ENS_15FilterOperation13OperationTypeE(WebCore::DefaultFilterOperation* self, std::uint32_t representedType) {
    ConstructDefaultFilterOperation(Checked(self), representedType);
}

void APS5_VABI _ZN7WebCore22DefaultFilterOperationC2ENS_15FilterOperation13OperationTypeE(WebCore::DefaultFilterOperation* self, std::uint32_t representedType) {
    ConstructDefaultFilterOperation(Checked(self), representedType);
}

WTF::RefPtr<WebCore::FilterOperation> APS5_VABI _ZN7WebCore30InvertLightnessFilterOperation5blendEPKNS_15FilterOperationEdb(WebCore::InvertLightnessFilterOperation* self, const WebCore::FilterOperation* from, double, bool) {
    Checked(self);
    if (from && from->m_type != self->m_type)
        return WTF::RefPtr<WebCore::FilterOperation>(self);

    auto* operation = static_cast<WebCore::InvertLightnessFilterOperation*>(_ZN3WTF10fastMallocEm(sizeof(WebCore::InvertLightnessFilterOperation)));
    ConstructInvertLightnessFilterOperation(operation);
    return WTF::RefPtr<WebCore::FilterOperation>(*operation, WTF::AdoptRef);
}

bool APS5_VABI _ZNK7WebCore30InvertLightnessFilterOperation14transformColorERNS_5SRGBAIfEE(const WebCore::InvertLightnessFilterOperation* self, WebCore::SRGBA<float>& color) {
    Checked(self);
    auto hueRotated = HueRotate(ResolvedComponents(color), 0.5f);
    color = ClampedColor(TransformColorComponents(ToDarkModeMatrix, hueRotated));
    return true;
}

bool APS5_VABI _ZNK7WebCore30InvertLightnessFilterOperation21inverseTransformColorERNS_5SRGBAIfEE(const WebCore::InvertLightnessFilterOperation* self, WebCore::SRGBA<float>& color) {
    Checked(self);
    auto converted = TransformColorComponents(ToLightModeMatrix, ResolvedComponents(color));
    color = ClampedColor(HueRotate(converted, 0.5f));
    return true;
}

}
