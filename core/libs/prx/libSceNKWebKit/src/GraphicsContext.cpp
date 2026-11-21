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
#include "prx/libSceNKWebKit/include/GraphicsContextExports.hpp"
#include "prx/libcairo/include/Bridge.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using NKWebKit::GraphicsContextAbi::Context;
using NKWebKit::GraphicsContextAbi::ImagePaintingOptions;
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
    CairoSetSourceRgba_nid_no_patch(context, red, green, blue, alpha);
}

void Rectangle(CairoContext* context, const WebCore::FloatRect& rect) {
    CairoRectangle_nid_no_patch(context, rect.x(), rect.y(), rect.width(), rect.height());
}

void AppendPath(CairoContext* context, const void* path) {
    if (!path) throw std::invalid_argument("WebCore::GraphicsContext path is null");
    auto* pathContext = *static_cast<CairoContext* const*>(path);
    CairoNewPath_nid_no_patch(context);
    if (!pathContext) return;
    CairoPath* copy = CairoCopyPath_nid_no_patch(pathContext);
    if (!copy) return;
    CairoAppendPath_nid_no_patch(context, copy);
    CairoPathDestroy_nid_no_patch(copy);
}

void ApplyAntialias(Context& context) {
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    CairoSetAntialias_nid_no_patch(cr, (context.state.flags & AntialiasFlag) ? 0 : 1);
}

int CairoCompositeOperator(const std::uint8_t compositeOperator) {
    if (compositeOperator <= 5) return compositeOperator;
    if (compositeOperator <= 10) return compositeOperator + 1;
    switch (compositeOperator) {
    case 11:
        return 17;
    case 12:
        return 12;
    case 13:
        return 23;
    default:
        throw std::invalid_argument("unsupported CompositeOperator");
    }
}

int CairoOperator(const std::uint8_t compositeOperator, const std::uint8_t blendMode) {
    switch (blendMode) {
    case 1:
        return CairoCompositeOperator(compositeOperator);
    case 2:
        return 14;
    case 3:
        return 15;
    case 4:
        return 17;
    case 5:
        return 18;
    case 6:
        return 16;
    case 7:
        return 19;
    case 8:
        return 20;
    case 9:
        return 21;
    case 10:
        return 22;
    case 11:
        return 23;
    case 12:
        return 24;
    case 13:
        return 25;
    case 14:
        return 26;
    case 15:
        return 27;
    case 16:
        return 28;
    case 17:
    case 18:
        return 2;
    default:
        throw std::invalid_argument("unsupported BlendMode");
    }
}

void ApplyOperator(Context& context) {
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    CairoSetOperator_nid_no_patch(cr, CairoOperator(context.state.compositeOperator, context.state.blendMode));
}

WebCore::Color MultiplyAlpha(const WebCore::Color& color, float shade) {
    if (IsOutOfLine(color)) throw std::invalid_argument("out-of-line WebCore::Color is not supported");
    const auto alpha = static_cast<std::uint64_t>(std::lround(static_cast<double>(color.m_colorAndFlags & 0xFF) * shade));
    return WebCore::Color{(color.m_colorAndFlags & ~0xFFULL) | alpha};
}

struct LineBounds {
    WebCore::FloatRect m_rect;
    WebCore::Color m_color;
};

LineBounds ComputeLineBounds(Context& context, const WebCore::FloatPoint& point, float width, bool printing, WebCore::Color color, float strokeThickness) {
    WebCore::FloatPoint origin = point;
    const float thickness = std::max(strokeThickness, 0.5f);
    if (printing) return {{origin, {width, thickness}}, color};
    CairoMatrix matrix{};
    CairoGetMatrix_nid_no_patch(Cairo(context), &matrix);
    const double scale = matrix.yx != 0.0 ? std::hypot(matrix.xx, matrix.yx) : matrix.xx;
    if (scale < 1.0) color = MultiplyAlpha(color, static_cast<float>(std::max(scale, 0.4)));
    const double deviceX = matrix.xx * point.m_x + matrix.xy * point.m_y + matrix.x0;
    const double deviceY = matrix.yx * point.m_x + matrix.yy * point.m_y + matrix.y0;
    const double deviceOriginX = std::round(deviceX);
    const double deviceOriginY = std::ceil(deviceY);
    const double determinant = matrix.xx * matrix.yy - matrix.yx * matrix.xy;
    if (determinant != 0.0) {
        const double inverseX = (matrix.yy * deviceOriginX - matrix.xy * deviceOriginY + matrix.xy * matrix.y0 - matrix.yy * matrix.x0) / determinant;
        const double inverseY = (-matrix.yx * deviceOriginX + matrix.xx * deviceOriginY - matrix.xx * matrix.y0 + matrix.yx * matrix.x0) / determinant;
        origin = {static_cast<float>(inverseX), static_cast<float>(inverseY)};
    }
    return {{origin, {width, thickness}}, color};
}

void DrawLinesForText(Context& context, const WebCore::FloatPoint& point, float thickness, const std::vector<double>& widths, bool printing, bool doubleUnderlines) {
    CairoContext* cr = Cairo(context);
    if (!cr || widths.empty() || widths.size() % 2) return;
    const LineBounds bounds = ComputeLineBounds(context, point, static_cast<float>(widths.back()), printing, context.state.strokeColor, thickness);
    ApplyOperator(context);
    CairoSave_nid_no_patch(cr);
    const auto fillDash = [&](double x, double width, double y) {
        if (width <= 0.0) return;
        SetSource(cr, bounds.m_color);
        CairoNewPath_nid_no_patch(cr);
        CairoRectangle_nid_no_patch(cr, bounds.m_rect.x() + x, bounds.m_rect.y() + y, width, bounds.m_rect.height());
        CairoFill_nid_no_patch(cr);
    };
    for (std::size_t i = 0; i < widths.size(); i += 2) fillDash(widths[i], widths[i + 1] - widths[i], 0.0);
    if (doubleUnderlines)
        for (std::size_t i = 0; i < widths.size(); i += 2) fillDash(widths[i], widths[i + 1] - widths[i], 2.0 * bounds.m_rect.height());
    CairoRestore_nid_no_patch(cr);
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
    CairoSave_nid_no_patch(cr);
    ApplyAntialias(context);
}

void APS5_VABI _ZN7WebCore15GraphicsContextC2EPNS_20PlatformContextCairoE(Context* self, PlatformContext* platform) {
    _ZN7WebCore15GraphicsContextC1EPNS_20PlatformContextCairoE(self, platform);
}

void APS5_VABI _ZN7WebCore15GraphicsContextD1Ev(Context* self) {
    auto& context = Checked(self, "WebCore::GraphicsContext::~GraphicsContext");
    auto* data = Private(context);
    if (data) {
        if (data->cr) CairoRestore_nid_no_patch(data->cr);
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
    CairoSave_nid_no_patch(data->cr);
    data->stack.push_back(context.state);
}

void APS5_VABI _ZN7WebCore15GraphicsContext7restoreEv(Context* self) {
    auto& context = Checked(self, "WebCore::GraphicsContext::restore");
    auto* data = Private(context);
    if (!data || !data->cr || data->stack.empty()) return;
    context.state = data->stack.back();
    data->stack.pop_back();
    CairoRestore_nid_no_patch(data->cr);
    ApplyAntialias(context);
}

void APS5_VABI _ZN7WebCore15GraphicsContext9translateEff(Context* self, float x, float y) {
    auto& context = Checked(self, "WebCore::GraphicsContext::translate");
    if (CairoContext* cr = Cairo(context)) CairoTranslate_nid_no_patch(cr, x, y);
}

void APS5_VABI _ZN7WebCore15GraphicsContext5scaleERKNS_9FloatSizeE(Context* self, const WebCore::FloatSize* scale) {
    auto& context = Checked(self, "WebCore::GraphicsContext::scale");
    if (!scale) throw std::invalid_argument("WebCore::GraphicsContext::scale requires a scale");
    if (CairoContext* cr = Cairo(context)) CairoScale_nid_no_patch(cr, scale->m_width, scale->m_height);
}

void APS5_VABI _ZN7WebCore15GraphicsContext22applyDeviceScaleFactorEf(Context* self, float scale) {
    auto& context = Checked(self, "WebCore::GraphicsContext::applyDeviceScaleFactor");
    if (CairoContext* cr = Cairo(context)) CairoScale_nid_no_patch(cr, scale, scale);
}

void APS5_VABI _ZN7WebCore15GraphicsContext4clipERKNS_9FloatRectE(Context* self, const WebCore::FloatRect* rect) {
    auto& context = Checked(self, "WebCore::GraphicsContext::clip");
    if (!rect) throw std::invalid_argument("WebCore::GraphicsContext::clip requires a rectangle");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    CairoNewPath_nid_no_patch(cr);
    Rectangle(cr, *rect);
    CairoClip_nid_no_patch(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectE(Context* self, const WebCore::FloatRect* rect) {
    auto& context = Checked(self, "WebCore::GraphicsContext::fillRect");
    if (!rect) throw std::invalid_argument("WebCore::GraphicsContext::fillRect requires a rectangle");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    ApplyOperator(context);
    SetSource(cr, context.state.fillColor);
    CairoNewPath_nid_no_patch(cr);
    Rectangle(cr, *rect);
    CairoFill_nid_no_patch(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(Context* self, const WebCore::FloatRect* rect, const WebCore::Color* color) {
    auto& context = Checked(self, "WebCore::GraphicsContext::fillRect");
    if (!rect || !color) throw std::invalid_argument("WebCore::GraphicsContext::fillRect requires a rectangle and a color");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    ApplyOperator(context);
    SetSource(cr, *color);
    CairoNewPath_nid_no_patch(cr);
    Rectangle(cr, *rect);
    CairoFill_nid_no_patch(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext9clearRectERKNS_9FloatRectE(Context* self, const WebCore::FloatRect* rect) {
    auto& context = Checked(self, "WebCore::GraphicsContext::clearRect");
    if (!rect) throw std::invalid_argument("WebCore::GraphicsContext::clearRect requires a rectangle");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    CairoSave_nid_no_patch(cr);
    CairoSetOperator_nid_no_patch(cr, 0);
    CairoNewPath_nid_no_patch(cr);
    Rectangle(cr, *rect);
    CairoFill_nid_no_patch(cr);
    CairoRestore_nid_no_patch(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext8fillPathERKNS_4PathE(Context* self, const void* path) {
    auto& context = Checked(self, "WebCore::GraphicsContext::fillPath");
    if (!path) throw std::invalid_argument("WebCore::GraphicsContext::fillPath requires a path");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    ApplyOperator(context);
    SetSource(cr, context.state.fillColor);
    AppendPath(cr, path);
    CairoFill_nid_no_patch(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext10strokePathERKNS_4PathE(Context* self, const void* path) {
    auto& context = Checked(self, "WebCore::GraphicsContext::strokePath");
    if (!path) throw std::invalid_argument("WebCore::GraphicsContext::strokePath requires a path");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    ApplyOperator(context);
    SetSource(cr, context.state.strokeColor);
    CairoSetLineWidth_nid_no_patch(cr, context.state.strokeThickness);
    AppendPath(cr, path);
    CairoStroke_nid_no_patch(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext10strokeRectERKNS_9FloatRectEf(Context* self, const WebCore::FloatRect* rect, float thickness) {
    auto& context = Checked(self, "WebCore::GraphicsContext::strokeRect");
    if (!rect) throw std::invalid_argument("WebCore::GraphicsContext::strokeRect requires a rectangle");
    CairoContext* cr = Cairo(context);
    if (!cr) return;
    ApplyOperator(context);
    SetSource(cr, context.state.strokeColor);
    CairoSetLineWidth_nid_no_patch(cr, thickness);
    CairoNewPath_nid_no_patch(cr);
    Rectangle(cr, *rect);
    CairoStroke_nid_no_patch(cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext15drawNativeImageERKN3WTF6RefPtrI14_cairo_surfaceNS1_13DumbPtrTraitsIS3_EEEERKNS_9FloatSizeERKNS_9FloatRectESE_RKNS_20ImagePaintingOptionsE(Context* self, const void* image, const WebCore::FloatSize*, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const ImagePaintingOptions* options) {
    auto& context = Checked(self, "WebCore::GraphicsContext::drawNativeImage");
    if (!image || !destination || !source || !options) throw std::invalid_argument("WebCore::GraphicsContext::drawNativeImage requires an image, rectangles and options");
    auto* surface = *static_cast<CairoSurface* const*>(image);
    CairoContext* cr = Cairo(context);
    if (!cr || !surface) return;
    if (destination->width() <= 0 || destination->height() <= 0 || source->width() <= 0 || source->height() <= 0) return;
    if (options->orientation != 1) throw std::invalid_argument("only the default ImageOrientation is supported");
    if (options->interpolationQuality != 0) throw std::invalid_argument("only the default interpolation quality is supported");
    const double scaleX = destination->width() / source->width();
    const double scaleY = destination->height() / source->height();
    CairoSave_nid_no_patch(cr);
    CairoSetOperator_nid_no_patch(cr, CairoOperator(options->compositeOperator, options->blendMode));
    CairoTranslate_nid_no_patch(cr, destination->x() - source->x() * scaleX, destination->y() - source->y() * scaleY);
    CairoScale_nid_no_patch(cr, scaleX, scaleY);
    CairoSetSourceSurface_nid_no_patch(cr, surface, 0.0, 0.0);
    CairoNewPath_nid_no_patch(cr);
    Rectangle(cr, *source);
    CairoFill_nid_no_patch(cr);
    CairoRestore_nid_no_patch(cr);
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

void APS5_VABI _ZN7WebCore15GraphicsContext21setCompositeOperationENS_17CompositeOperatorENS_9BlendModeE(Context* self, std::uint8_t compositeOperator, std::uint8_t blendMode) {
    auto& context = Checked(self, "WebCore::GraphicsContext::setCompositeOperation");
    CairoOperator(compositeOperator, blendMode);
    context.state.compositeOperator = compositeOperator;
    context.state.blendMode = blendMode;
    if (CairoContext* cr = Cairo(context)) CairoSetOperator_nid_no_patch(cr, CairoOperator(compositeOperator, blendMode));
}

void APS5_VABI _ZN7WebCore15GraphicsContext22beginTransparencyLayerEf(Context* self, float opacity) {
    auto& context = Checked(self, "WebCore::GraphicsContext::beginTransparencyLayer");
    auto* data = Private(context);
    if (!data || !data->cr) return;
    _ZN7WebCore15GraphicsContext4saveEv(self);
    context.state.alpha = opacity;
    ++context.transparencyCount;
    CairoPushGroup_nid_no_patch(data->cr);
}

void APS5_VABI _ZN7WebCore15GraphicsContext20endTransparencyLayerEv(Context* self) {
    auto& context = Checked(self, "WebCore::GraphicsContext::endTransparencyLayer");
    auto* data = Private(context);
    if (!data || !data->cr || context.transparencyCount == 0 || data->stack.empty()) return;
    CairoPopGroupToSource_nid_no_patch(data->cr);
    ApplyOperator(context);
    CairoPaintWithAlpha_nid_no_patch(data->cr, context.state.alpha);
    --context.transparencyCount;
    _ZN7WebCore15GraphicsContext7restoreEv(self);
}

void APS5_VABI _ZN7WebCore15GraphicsContext15drawLineForTextERKNS_9FloatRectEbbNS_11StrokeStyleE(Context* self, const WebCore::FloatRect* rect, bool printing, bool doubleUnderlines, std::uint8_t) {
    auto& context = Checked(self, "WebCore::GraphicsContext::drawLineForText");
    if (!rect) throw std::invalid_argument("WebCore::GraphicsContext::drawLineForText requires a rectangle");
    if (!Cairo(context)) return;
    const std::vector<double> widths{0.0, static_cast<double>(rect->width())};
    DrawLinesForText(context, {rect->x(), rect->y()}, rect->height(), widths, printing, doubleUnderlines);
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
    CairoGetMatrix_nid_no_patch(cr, &matrix);
    transform.m_transform = {matrix.xx, matrix.yx, matrix.xy, matrix.yy, matrix.x0, matrix.y0};
    return transform;
}

}
