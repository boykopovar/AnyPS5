// Derived from WebKit/WebKit/Source/WebCore/platform/graphics/filters/FilterOperation.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "prx/libSceNKWebKit/include/ComponentTransferFilterExports.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

template<typename Functor>
void ModifyEachNonAlphaComponent(WebCore::SRGBA<float>& color, Functor&& functor) {
    color.red = static_cast<float>(functor(color.red));
    color.green = static_cast<float>(functor(color.green));
    color.blue = static_cast<float>(functor(color.blue));
}

}  // namespace

extern "C" {

bool APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(const NKWebKit::ComponentTransferFilterOperationAbi* self, WebCore::SRGBA<float>* colorPointer) {
    if (!self) throw std::invalid_argument("BasicComponentTransferFilterOperation::transformColor: null this");
    if (!colorPointer) throw std::invalid_argument("BasicComponentTransferFilterOperation::transformColor: null color");
    WebCore::SRGBA<float>& color = *colorPointer;
    const double amount = self->amount;
    switch (self->type) {
    case NKWebKit::LegacyFilterOperationType::Opacity:
        color.alpha = static_cast<float>(color.alpha * amount);
        return true;
    case NKWebKit::LegacyFilterOperationType::Invert: {
        const float oneMinusAmount = static_cast<float>(1.0f - amount);
        ModifyEachNonAlphaComponent(color, [&](float component) {
            return 1.0f - (oneMinusAmount + component * (amount - oneMinusAmount));
        });
        return true;
    }
    case NKWebKit::LegacyFilterOperationType::Contrast: {
        const float intercept = static_cast<float>(-(0.5f * amount) + 0.5f);
        ModifyEachNonAlphaComponent(color, [&](float component) {
            return std::clamp<float>(static_cast<float>(intercept + amount * component), 0.0f, 1.0f);
        });
        return true;
    }
    case NKWebKit::LegacyFilterOperationType::Brightness:
        ModifyEachNonAlphaComponent(color, [&](float component) {
            return std::max<float>(static_cast<float>(amount * component), 0.0f);
        });
        return true;
    default:
        return false;
    }
}

}
