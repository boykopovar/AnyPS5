#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/GraphicsContextAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore20GraphicsContextStateC1Ev(NKWebKit::GraphicsContextAbi::State* self);
void APS5_VABI _ZN7WebCore20GraphicsContextStateC2Ev(NKWebKit::GraphicsContextAbi::State* self);
void APS5_VABI _ZN7WebCore20GraphicsContextStateD1Ev(NKWebKit::GraphicsContextAbi::State* self);
void APS5_VABI _ZN7WebCore20GraphicsContextStateD2Ev(NKWebKit::GraphicsContextAbi::State* self);

void APS5_VABI _ZN7WebCore15GraphicsContextC1EPNS_20PlatformContextCairoE(NKWebKit::GraphicsContextAbi::Context* self, NKWebKit::GraphicsContextAbi::PlatformContext* platform);
void APS5_VABI _ZN7WebCore15GraphicsContextC2EPNS_20PlatformContextCairoE(NKWebKit::GraphicsContextAbi::Context* self, NKWebKit::GraphicsContextAbi::PlatformContext* platform);
void APS5_VABI _ZN7WebCore15GraphicsContextD1Ev(NKWebKit::GraphicsContextAbi::Context* self);
void APS5_VABI _ZN7WebCore15GraphicsContextD2Ev(NKWebKit::GraphicsContextAbi::Context* self);

void APS5_VABI _ZN7WebCore15GraphicsContext4saveEv(NKWebKit::GraphicsContextAbi::Context* self);
void APS5_VABI _ZN7WebCore15GraphicsContext7restoreEv(NKWebKit::GraphicsContextAbi::Context* self);
void APS5_VABI _ZN7WebCore15GraphicsContext9translateEff(NKWebKit::GraphicsContextAbi::Context* self, float x, float y);
void APS5_VABI _ZN7WebCore15GraphicsContext5scaleERKNS_9FloatSizeE(NKWebKit::GraphicsContextAbi::Context* self, const WebCore::FloatSize* scale);
void APS5_VABI _ZN7WebCore15GraphicsContext22applyDeviceScaleFactorEf(NKWebKit::GraphicsContextAbi::Context* self, float scale);
void APS5_VABI _ZN7WebCore15GraphicsContext4clipERKNS_9FloatRectE(NKWebKit::GraphicsContextAbi::Context* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectE(NKWebKit::GraphicsContextAbi::Context* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore15GraphicsContext8fillRectERKNS_9FloatRectERKNS_5ColorE(NKWebKit::GraphicsContextAbi::Context* self, const WebCore::FloatRect* rect, const WebCore::Color* color);
void APS5_VABI _ZN7WebCore15GraphicsContext9clearRectERKNS_9FloatRectE(NKWebKit::GraphicsContextAbi::Context* self, const WebCore::FloatRect* rect);
void APS5_VABI _ZN7WebCore15GraphicsContext8fillPathERKNS_4PathE(NKWebKit::GraphicsContextAbi::Context* self, const void* path);
void APS5_VABI _ZN7WebCore15GraphicsContext10strokePathERKNS_4PathE(NKWebKit::GraphicsContextAbi::Context* self, const void* path);
void APS5_VABI _ZN7WebCore15GraphicsContext10strokeRectERKNS_9FloatRectEf(NKWebKit::GraphicsContextAbi::Context* self, const WebCore::FloatRect* rect, float thickness);
void APS5_VABI _ZN7WebCore15GraphicsContext15drawNativeImageERKN3WTF6RefPtrI14_cairo_surfaceNS1_13DumbPtrTraitsIS3_EEEERKNS_9FloatSizeERKNS_9FloatRectESE_RKNS_20ImagePaintingOptionsE(NKWebKit::GraphicsContextAbi::Context* self, const void* image, const WebCore::FloatSize* imageSize, const WebCore::FloatRect* destination, const WebCore::FloatRect* source, const NKWebKit::GraphicsContextAbi::ImagePaintingOptions* options);
void APS5_VABI _ZN7WebCore15GraphicsContext12setFillColorERKNS_5ColorE(NKWebKit::GraphicsContextAbi::Context* self, const WebCore::Color* color);
void APS5_VABI _ZN7WebCore15GraphicsContext14setStrokeColorERKNS_5ColorE(NKWebKit::GraphicsContextAbi::Context* self, const WebCore::Color* color);
void APS5_VABI _ZN7WebCore15GraphicsContext18setStrokeThicknessEf(NKWebKit::GraphicsContextAbi::Context* self, float thickness);
void APS5_VABI _ZN7WebCore15GraphicsContext18setShouldAntialiasEb(NKWebKit::GraphicsContextAbi::Context* self, bool enabled);
void APS5_VABI _ZN7WebCore15GraphicsContext20setShouldSmoothFontsEb(NKWebKit::GraphicsContextAbi::Context* self, bool enabled);
void APS5_VABI _ZN7WebCore15GraphicsContext28setImageInterpolationQualityENS_20InterpolationQualityE(NKWebKit::GraphicsContextAbi::Context* self, std::uint8_t quality);
void APS5_VABI _ZN7WebCore15GraphicsContext9setShadowERKNS_9FloatSizeEfRKNS_5ColorE(NKWebKit::GraphicsContextAbi::Context* self, const WebCore::FloatSize* offset, float blur, const WebCore::Color* color);
void APS5_VABI _ZN7WebCore15GraphicsContext11clearShadowEv(NKWebKit::GraphicsContextAbi::Context* self);
void APS5_VABI _ZN7WebCore15GraphicsContext21setCompositeOperationENS_17CompositeOperatorENS_9BlendModeE(NKWebKit::GraphicsContextAbi::Context* self, std::uint8_t compositeOperator, std::uint8_t blendMode);
void APS5_VABI _ZN7WebCore15GraphicsContext22beginTransparencyLayerEf(NKWebKit::GraphicsContextAbi::Context* self, float opacity);
void APS5_VABI _ZN7WebCore15GraphicsContext20endTransparencyLayerEv(NKWebKit::GraphicsContextAbi::Context* self);
void APS5_VABI _ZN7WebCore15GraphicsContext15drawLineForTextERKNS_9FloatRectEbbNS_11StrokeStyleE(NKWebKit::GraphicsContextAbi::Context* self, const WebCore::FloatRect* rect, bool printing, bool doubleUnderlines, std::uint8_t strokeStyle);
bool APS5_VABI _ZNK7WebCore15GraphicsContext18hasPlatformContextEv(const NKWebKit::GraphicsContextAbi::Context* self);
NKWebKit::GraphicsContextAbi::PlatformContext* APS5_VABI _ZNK7WebCore15GraphicsContext15platformContextEv(const NKWebKit::GraphicsContextAbi::Context* self);
WebCore::AffineTransform APS5_VABI _ZNK7WebCore15GraphicsContext6getCTMENS0_18IncludeDeviceScaleE(const NKWebKit::GraphicsContextAbi::Context* self, std::uint8_t includeDeviceScale);

}
