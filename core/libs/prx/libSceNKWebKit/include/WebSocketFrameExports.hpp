#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebSocketFrameAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore14WebSocketFrameC1ENS0_6OpCodeEbbbPKcm(
    NKWebKit::WebCoreAbi::LegacyWebSocketFrame* self, std::uint32_t opCode, bool final, bool compress, bool masked, const char* payload, std::size_t payloadLength);
void APS5_VABI _ZN7WebCore14WebSocketFrameC2ENS0_6OpCodeEbbbPKcm(
    NKWebKit::WebCoreAbi::LegacyWebSocketFrame* self, std::uint32_t opCode, bool final, bool compress, bool masked, const char* payload, std::size_t payloadLength);

}
