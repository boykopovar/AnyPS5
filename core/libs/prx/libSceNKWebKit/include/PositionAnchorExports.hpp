#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/PositionAbi.hpp"

#include <cstdint>

extern "C" {

void APS5_VABI _ZN7WebCore8PositionC1EPNS_4NodeENS0_10AnchorTypeE(NKWebKit::WebCoreAbi::Position* self, NKWebKit::WebCoreAbi::Node* anchorNode, std::uint32_t anchorType);
void APS5_VABI _ZN7WebCore8PositionC2EPNS_4NodeENS0_10AnchorTypeE(NKWebKit::WebCoreAbi::Position* self, NKWebKit::WebCoreAbi::Node* anchorNode, std::uint32_t anchorType);
void APS5_VABI _ZN7WebCore8PositionC1EPNS_4NodeEjNS0_10AnchorTypeE(NKWebKit::WebCoreAbi::Position* self, NKWebKit::WebCoreAbi::Node* anchorNode, std::uint32_t offset, std::uint32_t anchorType);
void APS5_VABI _ZN7WebCore8PositionC2EPNS_4NodeEjNS0_10AnchorTypeE(NKWebKit::WebCoreAbi::Position* self, NKWebKit::WebCoreAbi::Node* anchorNode, std::uint32_t offset, std::uint32_t anchorType);
NKWebKit::WebCoreAbi::Node* APS5_VABI _ZNK7WebCore8Position13containerNodeEv(const NKWebKit::WebCoreAbi::Position* self);

}
