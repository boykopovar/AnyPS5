/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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
/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
/*
 * Derived from Source/WebCore/platform/graphics/filters/FilterOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/filters/FilterOperation.cpp@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/ColorMatrix.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/animation/AnimationUtilities.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceNKWebKit/include/BasicFilterOperationExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <new>

namespace {

constexpr std::uint32_t kOpGrayscale = 1;
constexpr std::uint32_t kOpSepia = 2;
constexpr std::uint32_t kOpSaturate = 3;
constexpr std::uint32_t kOpHueRotate = 4;
constexpr std::uint32_t kOpInvert = 5;
constexpr std::uint32_t kOpOpacity = 7;
constexpr std::uint32_t kOpBrightness = 8;
constexpr std::uint32_t kOpContrast = 9;

constexpr float kPiFloat = 3.14159265358979323846f;

struct RawFilterOperationHeader {
    void* vptr;
    std::uint32_t refCount;
    std::uint32_t type;
};

struct RawBasicFilterOperation {
    void* vptr;
    std::uint32_t refCount;
    std::uint32_t type;
    double amount;
};

static_assert(sizeof(RawFilterOperationHeader) == sizeof(WebCore::FilterOperation));
static_assert(offsetof(RawFilterOperationHeader, vptr) == offsetof(WebCore::FilterOperation, m_vptr));
static_assert(offsetof(RawFilterOperationHeader, refCount) == offsetof(WebCore::FilterOperation, m_refCount));
static_assert(offsetof(RawFilterOperationHeader, type) == offsetof(WebCore::FilterOperation, m_type));
static_assert(sizeof(RawBasicFilterOperation) == sizeof(WebCore::BasicColorMatrixFilterOperation));
static_assert(offsetof(RawBasicFilterOperation, amount) == sizeof(WebCore::FilterOperation));
static_assert(sizeof(RawBasicFilterOperation) == sizeof(WebCore::BasicComponentTransferFilterOperation));

struct ErrnoGuard {
    int saved{errno};
    ~ErrnoGuard() { errno = saved; }
};

template<typename T>
void ReadGuest(const void* source, T& out, const char* caller) {
    if (!source)
        NotImplemented_nid_no_patch(caller);
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(&out, source, sizeof(T)))
        NotImplemented_nid_no_patch(caller);
}

template<typename T>
void WriteGuest(void* destination, const T& value, const char* caller) {
    if (!destination)
        NotImplemented_nid_no_patch(caller);
    ErrnoGuard guard;
    if (!GuestCopyOut_nid_no_patch(destination, &value, sizeof(T)))
        NotImplemented_nid_no_patch(caller);
}

void RetainGuest(WebCore::FilterOperation* self, const char* caller) {
    if (!self)
        NotImplemented_nid_no_patch(caller);
    {
        ErrnoGuard guard;
        if (!GuestWritable_nid_no_patch(&self->m_refCount, sizeof(self->m_refCount)))
            NotImplemented_nid_no_patch(caller);
    }
    self->m_refCount.fetch_add(1, std::memory_order_relaxed);
}

double BlendDouble(double from, double to, double progress) {
    return from + (to - from) * progress;
}

double ColorMatrixPassthroughAmount(std::uint32_t type) {
    switch (type) {
    case kOpGrayscale:
    case kOpSepia:
    case kOpHueRotate:
        return 0.0;
    case kOpSaturate:
        return 1.0;
    default:
        return 0.0;
    }
}

double ComponentTransferPassthroughAmount(std::uint32_t type) {
    switch (type) {
    case kOpOpacity:
    case kOpContrast:
    case kOpBrightness:
        return 1.0;
    case kOpInvert:
        return 0.0;
    default:
        return 0.0;
    }
}

WebCore::SRGBA<float> ApplyColorMatrix3x3(const std::array<float, 9>& matrix, const WebCore::SRGBA<float>& input) {
    const std::array<float, 3> rgb{input.red, input.green, input.blue};
    std::array<float, 3> out{0.0f, 0.0f, 0.0f};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col)
            out[row] += matrix[row * 3 + col] * rgb[col];
    }
    return WebCore::SRGBA<float>{out[0], out[1], out[2], input.alpha};
}

std::array<float, 9> GrayscaleColorMatrix(float amount) {
    const float oneMinusAmount = std::clamp(1.0f - amount, 0.0f, 1.0f);
    return {
        0.2126f + 0.7874f * oneMinusAmount, 0.7152f - 0.7152f * oneMinusAmount, 0.0722f - 0.0722f * oneMinusAmount,
        0.2126f - 0.2126f * oneMinusAmount, 0.7152f + 0.2848f * oneMinusAmount, 0.0722f - 0.0722f * oneMinusAmount,
        0.2126f - 0.2126f * oneMinusAmount, 0.7152f - 0.7152f * oneMinusAmount, 0.0722f + 0.9278f * oneMinusAmount,
    };
}

std::array<float, 9> SepiaColorMatrix(float amount) {
    const float oneMinusAmount = std::clamp(1.0f - amount, 0.0f, 1.0f);
    return {
        0.393f + 0.607f * oneMinusAmount, 0.769f - 0.769f * oneMinusAmount, 0.189f - 0.189f * oneMinusAmount,
        0.349f - 0.349f * oneMinusAmount, 0.686f + 0.314f * oneMinusAmount, 0.168f - 0.168f * oneMinusAmount,
        0.272f - 0.272f * oneMinusAmount, 0.534f - 0.534f * oneMinusAmount, 0.131f + 0.869f * oneMinusAmount,
    };
}

std::array<float, 9> SaturationColorMatrix(float amount) {
    return {
        0.213f + 0.787f * amount, 0.715f - 0.715f * amount, 0.072f - 0.072f * amount,
        0.213f - 0.213f * amount, 0.715f + 0.285f * amount, 0.072f - 0.072f * amount,
        0.213f - 0.213f * amount, 0.715f - 0.715f * amount, 0.072f + 0.928f * amount,
    };
}

std::array<float, 9> HueRotateColorMatrix(float angleInDegrees) {
    const float radians = angleInDegrees * kPiFloat / 180.0f;
    const float cosHue = std::cos(radians);
    const float sinHue = std::sin(radians);
    return {
        0.213f + cosHue * 0.787f - sinHue * 0.213f, 0.715f - cosHue * 0.715f - sinHue * 0.715f, 0.072f - cosHue * 0.072f + sinHue * 0.928f,
        0.213f - cosHue * 0.213f + sinHue * 0.143f, 0.715f + cosHue * 0.285f + sinHue * 0.140f, 0.072f - cosHue * 0.072f - sinHue * 0.283f,
        0.213f - cosHue * 0.213f - sinHue * 0.787f, 0.715f - cosHue * 0.715f + sinHue * 0.715f, 0.072f + cosHue * 0.928f + sinHue * 0.072f,
    };
}

void APS5_VABI BasicFilterOperationCompleteDestructor(WebCore::FilterOperation*) {
}

void APS5_VABI BasicFilterOperationDeletingDestructor(WebCore::FilterOperation* self) {
    _ZN3WTF8fastFreeEPv(self);
}

bool APS5_VABI BasicFilterOperationReturnFalseColor(const WebCore::FilterOperation*, WebCore::SRGBA<float>&) {
    return false;
}

bool APS5_VABI BasicFilterOperationReturnFalse(const WebCore::FilterOperation*) {
    return false;
}

WebCore::BasicFilterOperationRef<WebCore::FilterOperation> APS5_VABI BasicFilterOperationColorMatrixClone(
    const WebCore::BasicColorMatrixFilterOperation* self) {
    RawBasicFilterOperation state{};
    ReadGuest(self, state, __func__);
    auto created = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(
        state.amount, state.type);
    return WebCore::BasicFilterOperationRef<WebCore::FilterOperation>(created.leakRef());
}

bool APS5_VABI BasicFilterOperationColorMatrixEquals(
    const WebCore::BasicColorMatrixFilterOperation* self, const WebCore::FilterOperation& other) {
    RawBasicFilterOperation left{};
    RawFilterOperationHeader rightHeader{};
    ReadGuest(self, left, __func__);
    ReadGuest(&other, rightHeader, __func__);
    if (left.type != rightHeader.type)
        return false;
    RawBasicFilterOperation right{};
    ReadGuest(&other, right, __func__);
    return left.amount == right.amount;
}

WebCore::BasicFilterOperationRef<WebCore::FilterOperation> APS5_VABI BasicFilterOperationComponentTransferClone(
    const WebCore::BasicComponentTransferFilterOperation* self) {
    RawBasicFilterOperation state{};
    ReadGuest(self, state, __func__);
    auto created = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(
        state.amount, state.type);
    return WebCore::BasicFilterOperationRef<WebCore::FilterOperation>(created.leakRef());
}

bool APS5_VABI BasicFilterOperationComponentTransferEquals(
    const WebCore::BasicComponentTransferFilterOperation* self, const WebCore::FilterOperation& other) {
    RawBasicFilterOperation left{};
    RawFilterOperationHeader rightHeader{};
    ReadGuest(self, left, __func__);
    ReadGuest(&other, rightHeader, __func__);
    if (left.type != rightHeader.type)
        return false;
    RawBasicFilterOperation right{};
    ReadGuest(&other, right, __func__);
    return left.amount == right.amount;
}

bool APS5_VABI BasicFilterOperationComponentTransferAffectsOpacity(const WebCore::BasicComponentTransferFilterOperation* self) {
    RawFilterOperationHeader header{};
    ReadGuest(self, header, __func__);
    return header.type == kOpOpacity;
}

void* gColorMatrixVtable[12] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&BasicFilterOperationCompleteDestructor),
    reinterpret_cast<void*>(&BasicFilterOperationDeletingDestructor),
    reinterpret_cast<void*>(&BasicFilterOperationColorMatrixClone),
    reinterpret_cast<void*>(&BasicFilterOperationColorMatrixEquals),
    reinterpret_cast<void*>(&_ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb),
    reinterpret_cast<void*>(&_ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE),
    reinterpret_cast<void*>(&BasicFilterOperationReturnFalseColor),
    reinterpret_cast<void*>(&BasicFilterOperationReturnFalse),
    reinterpret_cast<void*>(&BasicFilterOperationReturnFalse),
    reinterpret_cast<void*>(&BasicFilterOperationReturnFalse),
};

void* gComponentTransferVtable[12] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&BasicFilterOperationCompleteDestructor),
    reinterpret_cast<void*>(&BasicFilterOperationDeletingDestructor),
    reinterpret_cast<void*>(&BasicFilterOperationComponentTransferClone),
    reinterpret_cast<void*>(&BasicFilterOperationComponentTransferEquals),
    reinterpret_cast<void*>(&_ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb),
    reinterpret_cast<void*>(&_ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE),
    reinterpret_cast<void*>(&BasicFilterOperationReturnFalseColor),
    reinterpret_cast<void*>(&BasicFilterOperationComponentTransferAffectsOpacity),
    reinterpret_cast<void*>(&BasicFilterOperationReturnFalse),
    reinterpret_cast<void*>(&BasicFilterOperationReturnFalse),
};

void ConstructBasicFilterOperation(void* self, void* vptr, double amount, std::uint32_t type, const char* caller) {
    const RawBasicFilterOperation state{vptr, 1u, type, amount};
    WriteGuest(self, state, caller);
}

} // namespace

extern "C" {

void APS5_VABI _ZN7WebCore31BasicColorMatrixFilterOperationC1EdNS_15FilterOperation13OperationTypeE(
    WebCore::BasicColorMatrixFilterOperation* self, double amount, std::uint32_t type) {
    ConstructBasicFilterOperation(self, &gColorMatrixVtable[2], amount, type, __func__);
}

void APS5_VABI _ZN7WebCore31BasicColorMatrixFilterOperationC2EdNS_15FilterOperation13OperationTypeE(
    WebCore::BasicColorMatrixFilterOperation* self, double amount, std::uint32_t type) {
    ConstructBasicFilterOperation(self, &gColorMatrixVtable[2], amount, type, __func__);
}

WebCore::BasicFilterOperationRef<WebCore::BasicColorMatrixFilterOperation> APS5_VABI
_ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(
    double amount, std::uint32_t type) {
    auto* allocated = static_cast<WebCore::BasicColorMatrixFilterOperation*>(
        _ZN3WTF10fastMallocEm(sizeof(WebCore::BasicColorMatrixFilterOperation)));
    if (!allocated)
        throw std::bad_alloc();
    ConstructBasicFilterOperation(allocated, &gColorMatrixVtable[2], amount, type, __func__);
    return WebCore::BasicFilterOperationRef<WebCore::BasicColorMatrixFilterOperation>(allocated);
}

bool APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(
    const WebCore::BasicColorMatrixFilterOperation* self, WebCore::SRGBA<float>& color) {
    RawBasicFilterOperation state{};
    WebCore::SRGBA<float> localColor{};
    ReadGuest(self, state, __func__);
    ReadGuest(&color, localColor, __func__);

    const float amount = static_cast<float>(state.amount);
    switch (state.type) {
    case kOpGrayscale:
        localColor = ApplyColorMatrix3x3(GrayscaleColorMatrix(amount), localColor);
        break;
    case kOpSepia:
        localColor = ApplyColorMatrix3x3(SepiaColorMatrix(amount), localColor);
        break;
    case kOpHueRotate:
        localColor = ApplyColorMatrix3x3(HueRotateColorMatrix(amount), localColor);
        break;
    case kOpSaturate:
        localColor = ApplyColorMatrix3x3(SaturationColorMatrix(amount), localColor);
        break;
    default:
        return false;
    }

    WriteGuest(&color, localColor, __func__);
    return true;
}

WebCore::BasicFilterOperationRef<WebCore::FilterOperation> APS5_VABI
_ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb(
    WebCore::BasicColorMatrixFilterOperation* self, const WebCore::FilterOperation* from,
    double progress, bool blendToPassthrough) {
    RawBasicFilterOperation state{};
    ReadGuest(self, state, __func__);

    double fromAmount = ColorMatrixPassthroughAmount(state.type);
    if (from) {
        RawFilterOperationHeader fromHeader{};
        ReadGuest(from, fromHeader, __func__);
        if (fromHeader.type != state.type) {
            RetainGuest(self, __func__);
            return WebCore::BasicFilterOperationRef<WebCore::FilterOperation>(self);
        }
        if (!blendToPassthrough) {
            RawBasicFilterOperation fromState{};
            ReadGuest(from, fromState, __func__);
            fromAmount = fromState.amount;
        }
    }

    const double blendedAmount = blendToPassthrough
        ? BlendDouble(state.amount, ColorMatrixPassthroughAmount(state.type), progress)
        : BlendDouble(fromAmount, state.amount, progress);
    auto created = _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(
        blendedAmount, state.type);
    return WebCore::BasicFilterOperationRef<WebCore::FilterOperation>(created.leakRef());
}

void APS5_VABI _ZN7WebCore37BasicComponentTransferFilterOperationC1EdNS_15FilterOperation13OperationTypeE(
    WebCore::BasicComponentTransferFilterOperation* self, double amount, std::uint32_t type) {
    ConstructBasicFilterOperation(self, &gComponentTransferVtable[2], amount, type, __func__);
}

void APS5_VABI _ZN7WebCore37BasicComponentTransferFilterOperationC2EdNS_15FilterOperation13OperationTypeE(
    WebCore::BasicComponentTransferFilterOperation* self, double amount, std::uint32_t type) {
    ConstructBasicFilterOperation(self, &gComponentTransferVtable[2], amount, type, __func__);
}

WebCore::BasicFilterOperationRef<WebCore::BasicComponentTransferFilterOperation> APS5_VABI
_ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(
    double amount, std::uint32_t type) {
    auto* allocated = static_cast<WebCore::BasicComponentTransferFilterOperation*>(
        _ZN3WTF10fastMallocEm(sizeof(WebCore::BasicComponentTransferFilterOperation)));
    if (!allocated)
        throw std::bad_alloc();
    ConstructBasicFilterOperation(allocated, &gComponentTransferVtable[2], amount, type, __func__);
    return WebCore::BasicFilterOperationRef<WebCore::BasicComponentTransferFilterOperation>(allocated);
}

bool APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(
    const WebCore::BasicComponentTransferFilterOperation* self, WebCore::SRGBA<float>& color) {
    RawBasicFilterOperation state{};
    WebCore::SRGBA<float> localColor{};
    ReadGuest(self, state, __func__);
    ReadGuest(&color, localColor, __func__);

    const double amount = state.amount;
    switch (state.type) {
    case kOpOpacity:
        localColor.alpha = static_cast<float>(localColor.alpha * amount);
        break;
    case kOpInvert: {
        const float oneMinusAmount = static_cast<float>(1.0f - amount);
        const auto invertChannel = [amount, oneMinusAmount](float component) {
            return static_cast<float>(1.0f - (oneMinusAmount + component * (amount - oneMinusAmount)));
        };
        localColor.red = invertChannel(localColor.red);
        localColor.green = invertChannel(localColor.green);
        localColor.blue = invertChannel(localColor.blue);
        break;
    }
    case kOpContrast: {
        const float intercept = static_cast<float>(-(0.5f * amount) + 0.5f);
        const auto contrastChannel = [amount, intercept](float component) {
            return std::clamp<float>(static_cast<float>(intercept + amount * component), 0.0f, 1.0f);
        };
        localColor.red = contrastChannel(localColor.red);
        localColor.green = contrastChannel(localColor.green);
        localColor.blue = contrastChannel(localColor.blue);
        break;
    }
    case kOpBrightness: {
        const auto brightnessChannel = [amount](float component) {
            return std::max<float>(static_cast<float>(amount * component), 0.0f);
        };
        localColor.red = brightnessChannel(localColor.red);
        localColor.green = brightnessChannel(localColor.green);
        localColor.blue = brightnessChannel(localColor.blue);
        break;
    }
    default:
        return false;
    }

    WriteGuest(&color, localColor, __func__);
    return true;
}

WebCore::BasicFilterOperationRef<WebCore::FilterOperation> APS5_VABI
_ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb(
    WebCore::BasicComponentTransferFilterOperation* self, const WebCore::FilterOperation* from,
    double progress, bool blendToPassthrough) {
    RawBasicFilterOperation state{};
    ReadGuest(self, state, __func__);

    double fromAmount = ComponentTransferPassthroughAmount(state.type);
    if (from) {
        RawFilterOperationHeader fromHeader{};
        ReadGuest(from, fromHeader, __func__);
        if (fromHeader.type != state.type) {
            RetainGuest(self, __func__);
            return WebCore::BasicFilterOperationRef<WebCore::FilterOperation>(self);
        }
        if (!blendToPassthrough) {
            RawBasicFilterOperation fromState{};
            ReadGuest(from, fromState, __func__);
            fromAmount = fromState.amount;
        }
    }

    const double blendedAmount = blendToPassthrough
        ? BlendDouble(state.amount, ComponentTransferPassthroughAmount(state.type), progress)
        : BlendDouble(fromAmount, state.amount, progress);
    auto created = _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(
        blendedAmount, state.type);
    return WebCore::BasicFilterOperationRef<WebCore::FilterOperation>(created.leakRef());
}

}
