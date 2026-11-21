// Derived from Source/WebCore/dom/messageports/MessagePortChannel.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/MessagePortChannelExports.hpp"

using NKWebKit::WebCoreAbi::MessagePortChannel;
using NKWebKit::WebCoreAbi::MessagePortIdentifier;
using NKWebKit::WebCoreAbi::OptionalProcessIdentifier;

extern "C" {

OptionalProcessIdentifier APS5_VABI _ZN7WebCore18MessagePortChannel14processForPortERKNS_21MessagePortIdentifierE(MessagePortChannel* self, const MessagePortIdentifier* port) {
    const bool isFirstPort = port->processIdentifier == self->ports[0].processIdentifier && port->objectIdentifier == self->ports[0].objectIdentifier;
    return self->processes[isFirstPort ? 0 : 1];
}

bool APS5_VABI _ZNK7WebCore18MessagePortChannel31hasAnyMessagesPendingOrInFlightEv(const MessagePortChannel* self) {
    return self->messageBatchesInFlight || self->pendingMessages[0].size || self->pendingMessages[1].size;
}

}
