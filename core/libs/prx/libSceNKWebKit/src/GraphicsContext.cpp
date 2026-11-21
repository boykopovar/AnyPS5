// Derived from Source/WebCore/platform/graphics/GraphicsContext.cpp and Source/WebCore/platform/graphics/cairo/GraphicsContextCairo.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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
#include "prx/libSceNKWebKit/src/GraphicsContextAbi.hpp"
#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Matrix.hpp"
#include "prx/libcairo/include/Path.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using NKWebKit::GraphicsContextAbi::Context;
using NKWebKit::GraphicsContextAbi::PlatformContext;
using NKWebKit::GraphicsContextAbi::State;

constexpr std::uint64_t ColorFlagsShift = 48;
constexpr std::uint64_t OutOfLineFlag = 1ULL << (ColorFlagsShift + 3);
constexpr std::uint8_t AntialiasFlag = 1U << 0;
constexpr std::uint8_t SmoothFontsFlag = 1U << 1;
constexpr std::uint8_t SubpixelQuantizeFontsFlag = 1U << 2;

struct ContextPrivate {
    CairoContext* cr;
    PlatformContext* platform;
    std::vector<State> stack;
};

Context& Checked(Context* self, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + " requires a context");
    return *self;
}

const Context& Checked(const Context* self, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + " requires a context");
    return *self;
}

ContextPrivate* Private(Context& context) {
    return static_cast<ContextPrivate*>(context.data);
}

const ContextPrivate* Private(const Context& context) {
    return static_cast<const ContextPrivate*>(context.data);
}

CairoContext* Cairo(const Context& context) {
    const auto* data = Private(context);
    return data ? data->cr : nullptr;
}

State DefaultState() {
    State state{};
    state.shadowOffset = {0.0f, 0.0f};
    state.strokeThickness = 0.0f;
    state.shadowBlur = 0.0f;
    state.textDrawingMode = 1;
    state.strokeColor = WebCore::Color{0x000000FFULL};
    state.fillColor = WebCore::Color{0x000000FFULL};
    state.shadowColor = WebCore::Color{0};
    state.strokeStyle = 1;
    state.fillRule = 0;
    state.alpha = 1.0f;
    state.compositeOperator = 2;
    state.blendMode = 1;
    state.imageInterpolationQuality = 0;
    state.flags = AntialiasFlag | SmoothFontsFlag | SubpixelQuantizeFontsFlag;
    return state;
}

bool IsOutOfLine(const WebCore::Color& color) {
    return (color.m_colorAndFlags & OutOfLineFlag) != 0;
}

void SetSource(CairoContext* context, const WebCore::Color& color) {
    if (!context) return;
    if (IsOutOfLine(color)) throw std::invalid_argument("out-of-line WebCore::Color is not supported");
    const std::uint64_t packed = color.m_colorAndFlags;
    const double red = static_cast<double>((packed >> 24) & 0xFF) / 255.0;
    const double green = static_cast<double>((packed >> 16) & 0xFF) / 255.0;
    const double blue = static_cast<double>((packed >> 8) & 0xFF) / 255.0;
    const double alpha = static_cast<double>(packed & 0xFF) / 255.0;
    cairo_set_source_rgba(context, red, green, blue, alpha);
}

void Rectangle(CairoContext* context, const WebCore::FloatRect& rect) {
    cairo_rectangle(context, rect.x(), rect.y(), rect.width(), rect.height());
}

void AppendPath(CairoContext* context, const void* path) {
    if (!path) throw std::invalid_argument("WebCore::GraphicsContext path is null");
    auto* pathContext = *static_cast<CairoContext* const*>(path);
    cairo_new_path(context);
    if (!pathContext) return;
    CairoPath* copy = cairo_copy_path(pathContext);
    if (!copy) return;
    cairo_append_path(context, copy);
    cairo_path_destroy(copy);
}

void ApplyAntialias(Context& context) {
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    cairo_set_antialias(cr, (context.state.flags & AntialiasFlag) ? 0 : 1);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore20GraphicsContextStateC1Ev(State* self) {
    if (!self) throw std::invalid_argument("WebCore::GraphicsContextState::GraphicsContextState requires an object");
    *self = DefaultState();
}

void APS5_VABI _ZN7WebCore20GraphicsContextStateC2Ev(State* self) {
    if (!self) throw std::invalid_argument("WebCore::GraphicsContextState::GraphicsContextState requires an object");
    *self = DefaultState();
}

void APS5_VABI _ZN7WebCore20GraphicsContextStateD1Ev(State* self) {
    if (!self) throw std::invalid_argument("WebCore::GraphicsContextState::~GraphicsContextState requires an object");
}

void APS5_VABI _ZN7WebCore20GraphicsContextStateD2Ev(State* self) {
    if (!self) throw std::invalid_argument("WebCore::GraphicsContextState::~GraphicsContextState requires an object");
}

void APS5_VABI _ZN7WebCore15GraphicsContextC1EPNS_20PlatformContextCairoE(Context* self, PlatformContext* platform) {
    auto& context = Checked(self, "WebCore::GraphicsContext::GraphicsContext");
    context.data = nullptr;
    context.impl = nullptr;
    context.state = DefaultState();
    context.stackStorage = nullptr;
    context.stackCapacity = 0;
    context.stackSize = 0;
    context.paintInvalidationReasons = 0;
    context.transparencyCount = 0;
    context.contentfulPaintDetected = false;
    if (!platform) return;
    CairoContext* cr = static_cast<CairoContext*>(platform->cr);
    if (!cr) return;
    context.data = new ContextPrivate{cr, platform, {}};
    cairo_save(cr);
    ApplyAntialias(context);
}

void APS5_VABI _ZN7WebCore15GraphicsContextC2EPNS_20PlatformContextCairoE(Context* self, PlatformContext* platform) {
    _ZN7WebCore15GraphicsContextC1EPNS_20PlatformContextCairoE(self, platform);
}

void APS5_VABI _ZN7WebCore15GraphicsContextD1Ev(Context* self) {
    auto& context = Checked(self, "WebCore::GraphicsContext::~GraphicsContext");
    auto* data = Private(context);
    if (data) {
        if (data->cr) cairo_restore(data->cr);
        delete data;
    }
    context.data = nullptr;
    context.impl = nullptr;
}

void APS5_VABI _ZN7WebCore15GraphicsContextD2Ev(Context* self) {
    _ZN7WebCore15GraphicsContextD1Ev(self);
}

void APS5_VABI _ZN7WebCore15GraphicsContext4saveEv(Context* self) {
    auto& context = Checked(self, "WebCore::GraphicsContext::save");
    auto* data = Private(context);
    if (!data || !data->cr) return;
    cairo_save(data->cr);
    data->stack.push_back(context.state);
}

void APS5_VABI _ZN7WebCore15GraphicsContext7restoreEv(Context* self) {
    auto& context = Checked(self, "WebCore::GraphicsContext::restore");
    auto* data = Private(context);
    if (!data || !data->cr || data->stack.empty()) return;
    context.state = data->stack.back();
    data->stack.pop_back();
    cairo_restore(data->cr);
    ApplyAntialias(context);
}

void APS5_VABI _ZN7WebCore15GraphicsContext9translateEff(Context* self, float x, float y) {
    auto& context = Checked(self, "WebCore::GraphicsContext::translate");
    if (CairoContext* cr = Cairo(context)) cairo_translate(cr, x, y);
}

void APS5_VABI _ZN7WebCore15GraphicsContext5scaleERKNS_9FloatSizeE(Context* self, const WebCore::FloatSize* scale) {
    auto& context = Checked(self, "WebCore::GraphicsContext::scale");
    if (!scale) throw std::invalid_argument("WebCore::GraphicsContext::scale requires a scale");
    if (CairoContext* cr = Cairo(context)) cairo_scale(cr, scale->m_width, scale->m_height);
}

void APS5_VABI _ZN7WebCore15GraphicsContext22applyDeviceScaleFactorEf(Context* self, float scale) {
    auto& context = Checked(self, "WebCore::GraphicsContext::applyDeviceScaleFactor");
    if (CairoContext* cr = Cairo(context)) cairo_scale(cr, scale, scale);
}

void APS5_VABI _ZN7WebCore15GraphicsContext4clipERKNS_9FloatRectE(Context* self, const WebCore::FloatRect* rect) {
    auto& context = Checked(self, "WebCore::GraphicsContext::clip");
    if (!rect) throw std::invalid_argument("WebCore::GraphicsContext::clip requires a rectangle");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    cairo_new_path(cr);
    Rectangle(cr, *rect);
    cairo_clip(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectE(Context* self, const WebCore::FloatRect* rect) {
    auto& context = Checked(self, "WebCore::GraphicsContext::fillRect");
    if (!rect) throw std::invalid_argument("WebCore::GraphicsContext::fillRect requires a rectangle");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    SetSource(cr, context.state.fillColor);
    cairo_new_path(cr);
    Rectangle(cr, *rect);
    cairo_fill(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(Context* self, const WebCore::FloatRect* rect, const WebCore::Color* color) {
    auto& context = Checked(self, "WebCore::GraphicsContext::fillRect");
    if (!rect || !color) throw std::invalid_argument("WebCore::GraphicsContext::fillRect requires a rectangle and a color");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    SetSource(cr, *color);
    cairo_new_path(cr);
    Rectangle(cr, *rect);
    cairo_fill(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext9clearRectERKNS_9FloatRectE(Context* self, const WebCore::FloatRect* rect) {
    auto& context = Checked(self, "WebCore::GraphicsContext::clearRect");
    if (!rect) throw std::invalid_argument("WebCore::GraphicsContext::clearRect requires a rectangle");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    cairo_save(cr);
    cairo_set_operator(cr, 0);
    cairo_new_path(cr);
    Rectangle(cr, *rect);
    cairo_fill(cr);
    cairo_restore(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext8fillPathERKNS_4PathE(Context* self, const void* path) {
    auto& context = Checked(self, "WebCore::GraphicsContext::fillPath");
    if (!path) throw std::invalid_argument("WebCore::GraphicsContext::fillPath requires a path");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    SetSource(cr, context.state.fillColor);
    AppendPath(cr, path);
    cairo_fill(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext10strokePathERKNS_4PathE(Context* self, const void* path) {
    auto& context = Checked(self, "WebCore::GraphicsContext::strokePath");
    if (!path) throw std::invalid_argument("WebCore::GraphicsContext::strokePath requires a path");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    SetSource(cr, context.state.strokeColor);
    cairo_set_line_width(cr, context.state.strokeThickness);
    AppendPath(cr, path);
    cairo_stroke(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext12setFillColorERKNS_5ColorE(Context* self, const WebCore::Color* color) {
    auto& context = Checked(self, "WebCore::GraphicsContext::setFillColor");
    if (!color) throw std::invalid_argument("WebCore::GraphicsContext::setFillColor requires a color");
    if (IsOutOfLine(*color)) throw std::invalid_argument("out-of-line WebCore::Color is not supported");
    context.state.fillColor = *color;
}

void APS5_VABI _ZN7WebCore15GraphicsContext14setStrokeColorERKNS_5ColorE(Context* self, const WebCore::Color* color) {
    auto& context = Checked(self, "WebCore::GraphicsContext::setStrokeColor");
    if (!color) throw std::invalid_argument("WebCore::GraphicsContext::setStrokeColor requires a color");
    if (IsOutOfLine(*color)) throw std::invalid_argument("out-of-line WebCore::Color is not supported");
    context.state.strokeColor = *color;
}

void APS5_VABI _ZN7WebCore15GraphicsContext18setStrokeThicknessEf(Context* self, float thickness) {
    auto& context = Checked(self, "WebCore::GraphicsContext::setStrokeThickness");
    context.state.strokeThickness = thickness;
}

void APS5_VABI _ZN7WebCore15GraphicsContext18setShouldAntialiasEb(Context* self, bool enabled) {
    auto& context = Checked(self, "WebCore::GraphicsContext::setShouldAntialias");
    context.state.flags = enabled ? (context.state.flags | AntialiasFlag) : (context.state.flags & ~AntialiasFlag);
    ApplyAntialias(context);
}

void APS5_VABI _ZN7WebCore15GraphicsContext20setShouldSmoothFontsEb(Context* self, bool enabled) {
    auto& context = Checked(self, "WebCore::GraphicsContext::setShouldSmoothFonts");
    context.state.flags = enabled ? (context.state.flags | SmoothFontsFlag) : (context.state.flags & ~SmoothFontsFlag);
}

void APS5_VABI _ZN7WebCore15GraphicsContext28setImageInterpolationQualityENS_20InterpolationQualityE(Context* self, std::uint8_t quality) {
    auto& context = Checked(self, "WebCore::GraphicsContext::setImageInterpolationQuality");
    context.state.imageInterpolationQuality = quality;
}

void APS5_VABI _ZN7WebCore15GraphicsContext9setShadowERKNS_9FloatSizeEfRKNS_5ColorE(Context* self, const WebCore::FloatSize* offset, float blur, const WebCore::Color* color) {
    auto& context = Checked(self, "WebCore::GraphicsContext::setShadow");
    if (!offset || !color) throw std::invalid_argument("WebCore::GraphicsContext::setShadow requires an offset and a color");
    if (IsOutOfLine(*color)) throw std::invalid_argument("out-of-line WebCore::Color is not supported");
    context.state.shadowOffset = *offset;
    context.state.shadowBlur = blur;
    context.state.shadowColor = *color;
}

void APS5_VABI _ZN7WebCore15GraphicsContext11clearShadowEv(Context* self) {
    auto& context = Checked(self, "WebCore::GraphicsContext::clearShadow");
    context.state.shadowOffset = {0.0f, 0.0f};
    context.state.shadowBlur = 0.0f;
    context.state.shadowColor = WebCore::Color{0};
}

bool APS5_VABI _ZNK7WebCore15GraphicsContext18hasPlatformContextEv(const Context* self) {
    return Private(Checked(self, "WebCore::GraphicsContext::hasPlatformContext")) != nullptr;
}

PlatformContext* APS5_VABI _ZNK7WebCore15GraphicsContext15platformContextEv(const Context* self) {
    const auto& context = Checked(self, "WebCore::GraphicsContext::platformContext");
    const auto* data = Private(context);
    return data ? data->platform : nullptr;
}

WebCore::AffineTransform APS5_VABI _ZNK7WebCore15GraphicsContext6getCTMENS0_18IncludeDeviceScaleE(const Context* self, std::uint8_t) {
    const auto& context = Checked(self, "WebCore::GraphicsContext::getCTM");
    WebCore::AffineTransform transform;
    CairoContext* cr = Cairo(context);
    if (!cr) return transform;
    CairoMatrix matrix{};
    cairo_get_matrix(cr, &matrix);
    transform.m_transform = {matrix.xx, matrix.yx, matrix.xy, matrix.yy, matrix.x0, matrix.y0};
    return transform;
}

}
