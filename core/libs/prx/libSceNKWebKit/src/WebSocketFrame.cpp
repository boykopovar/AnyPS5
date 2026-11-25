// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/Modules/websockets/WebSocketFrame.cpp
/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebSocketFrameExports.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::LegacyWebSocketFrame;

void Construct(LegacyWebSocketFrame* self, std::uint32_t opCode, bool final, bool compress, bool masked, const char* payload, std::size_t payloadLength) {
    if (!self) throw std::invalid_argument("WebCore::WebSocketFrame::WebSocketFrame: null this");
    self->opCode = opCode;
    self->final = final;
    self->compress = compress;
    self->reserved2 = false;
    self->reserved3 = false;
    self->masked = masked;
    self->payload = payload;
    self->payloadLength = payloadLength;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore14WebSocketFrameC1ENS0_6OpCodeEbbbPKcm(
    LegacyWebSocketFrame* self, std::uint32_t opCode, bool final, bool compress, bool masked, const char* payload, std::size_t payloadLength) {
    Construct(self, opCode, final, compress, masked, payload, payloadLength);
}

void APS5_VABI _ZN7WebCore14WebSocketFrameC2ENS0_6OpCodeEbbbPKcm(
    LegacyWebSocketFrame* self, std::uint32_t opCode, bool final, bool compress, bool masked, const char* payload, std::size_t payloadLength) {
    Construct(self, opCode, final, compress, masked, payload, payloadLength);
}

}
