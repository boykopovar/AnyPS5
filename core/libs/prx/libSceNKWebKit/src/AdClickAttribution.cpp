// Derived from WebKit/WebKit/Source/WebCore/loader/AdClickAttribution.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/AdClickAttributionExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

namespace {

constexpr double kMaxAgeSeconds = 24.0 * 60.0 * 60.0 * 7.0;

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore18AdClickAttribution13markAsExpiredEv(NKWebKit::WebCoreAbi::AdClickAttribution* self) {
    self->timeOfAdClick = WTF::WallTime{};
}

bool APS5_VABI _ZNK7WebCore18AdClickAttribution10hasExpiredEv(const NKWebKit::WebCoreAbi::AdClickAttribution* self) {
    return _ZN3WTF8WallTime3nowEv().m_value > self->timeOfAdClick.m_value + kMaxAgeSeconds;
}

bool APS5_VABI _ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(const NKWebKit::WebCoreAbi::AdClickAttribution* self, const NKWebKit::WebCoreAbi::AdClickAttribution* other) {
    if (!other->conversion.hasValue)
        return true;
    if (!self->conversion.hasValue)
        return false;
    return self->conversion.value.priority > other->conversion.value.priority;
}

void APS5_VABI _ZN7WebCore18AdClickAttribution20markConversionAsSentEv(NKWebKit::WebCoreAbi::AdClickAttribution* self) {
    if (self->conversion.hasValue)
        self->conversion.value.wasSent = true;
}

bool APS5_VABI _ZNK7WebCore18AdClickAttribution17wasConversionSentEv(const NKWebKit::WebCoreAbi::AdClickAttribution* self) {
    return self->conversion.hasValue && self->conversion.value.wasSent;
}

}
