#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <cstdint>

extern "C" {

WebCore::LinearSRGBA<float> APS5_VABI _ZN7WebCore13toLinearSRGBAERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color);
WebCore::SRGBA<float> APS5_VABI _ZN7WebCore7toSRGBAERKNS_11LinearSRGBAIfEE(const WebCore::LinearSRGBA<float>& color);
WebCore::LinearDisplayP3<float> APS5_VABI _ZN7WebCore17toLinearDisplayP3ERKNS_9DisplayP3IfEE(const WebCore::DisplayP3<float>& color);
WebCore::DisplayP3<float> APS5_VABI _ZN7WebCore11toDisplayP3ERKNS_15LinearDisplayP3IfEE(const WebCore::LinearDisplayP3<float>& color);
WebCore::SRGBA<float> APS5_VABI _ZN7WebCore7toSRGBAERKNS_9DisplayP3IfEE(const WebCore::DisplayP3<float>& color);
WebCore::DisplayP3<float> APS5_VABI _ZN7WebCore11toDisplayP3ERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color);
WebCore::HSLA<float> APS5_VABI _ZN7WebCore6toHSLAERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color);
WebCore::SRGBA<float> APS5_VABI _ZN7WebCore7toSRGBAERKNS_4HSLAIfEE(const WebCore::HSLA<float>& color);

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore19serializationForCSSENS_5SRGBAIhEE(WebCore::SRGBA<std::uint8_t> color);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore20serializationForHTMLENS_5SRGBAIhEE(WebCore::SRGBA<std::uint8_t> color);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore32serializationForRenderTreeAsTextENS_5SRGBAIhEE(WebCore::SRGBA<std::uint8_t> color);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore19serializationForCSSERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore20serializationForHTMLERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore32serializationForRenderTreeAsTextERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore19serializationForCSSERKNS_11LinearSRGBAIfEE(const WebCore::LinearSRGBA<float>& color);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore20serializationForHTMLERKNS_11LinearSRGBAIfEE(const WebCore::LinearSRGBA<float>& color);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore32serializationForRenderTreeAsTextERKNS_11LinearSRGBAIfEE(const WebCore::LinearSRGBA<float>& color);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore19serializationForCSSERKNS_9DisplayP3IfEE(const WebCore::DisplayP3<float>& color);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore20serializationForHTMLERKNS_9DisplayP3IfEE(const WebCore::DisplayP3<float>& color);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore32serializationForRenderTreeAsTextERKNS_9DisplayP3IfEE(const WebCore::DisplayP3<float>& color);

}
