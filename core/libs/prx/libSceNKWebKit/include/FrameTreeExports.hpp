#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/FrameAbi.hpp"

extern "C" {

std::uint32_t APS5_VABI _ZNK7WebCore9FrameTree10childCountEv(const NKWebKit::WebCoreAbi::FrameTree* self);
NKWebKit::WebCoreAbi::Frame* APS5_VABI _ZNK7WebCore9FrameTree3topEv(const NKWebKit::WebCoreAbi::FrameTree* self);
NKWebKit::WebCoreAbi::Frame* APS5_VABI _ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(const NKWebKit::WebCoreAbi::FrameTree* self, const NKWebKit::WebCoreAbi::Frame* stayWithin);
bool APS5_VABI _ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(const NKWebKit::WebCoreAbi::FrameTree* self, const NKWebKit::WebCoreAbi::Frame* ancestor);

}
