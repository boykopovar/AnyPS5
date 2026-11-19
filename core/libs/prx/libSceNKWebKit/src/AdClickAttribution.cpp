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

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr double kMaxAgeSeconds = 24.0 * 60.0 * 60.0 * 7.0;
constexpr double kOneDaySeconds = 24.0 * 60.0 * 60.0;

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;

struct Builder {
    std::u16string units;
    bool is8Bit{true};

    void Append(std::string_view text) {
        units.append(text.begin(), text.end());
        Check();
    }

    void Append(const NKWebKit::WtfAbi::StringImpl* impl) {
        const StringView view = NKWebKit::WtfAbi::ViewOf(impl);
        if (!view.length) return;
        if (!view.is8Bit && is8Bit && (view.length > 1 || NKWebKit::WtfAbi::CodeUnitAt(view, 0) > 0xff)) is8Bit = false;
        for (unsigned i = 0; i < view.length; ++i) units.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, i));
        Check();
    }

    void Append(std::uint32_t number) {
        char digits[10];
        const auto result = std::to_chars(digits, digits + sizeof(digits), number);
        Append(std::string_view(digits, static_cast<std::size_t>(result.ptr - digits)));
    }

    void Check() const {
        if (units.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) throw std::length_error("WebCore::AdClickAttribution string exceeds maximum length");
    }

    String Finish() const {
        void* characters = nullptr;
        String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(units.size()), is8Bit, characters));
        for (std::size_t i = 0; i < units.size(); ++i) {
            if (is8Bit) static_cast<unsigned char*>(characters)[i] = static_cast<unsigned char>(units[i]);
            else static_cast<char16_t*>(characters)[i] = units[i];
        }
        return result;
    }
};

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

String APS5_VABI _ZNK7WebCore18AdClickAttribution8toStringEv(const NKWebKit::WebCoreAbi::AdClickAttribution* self) {
    if (!self) throw std::invalid_argument("null WebCore::AdClickAttribution");
    Builder builder;
    builder.Append("Source: ");
    builder.Append(self->source);
    builder.Append("\nDestination: ");
    builder.Append(self->destination);
    builder.Append("\nCampaign ID: ");
    builder.Append(self->campaign);
    if (self->conversion.hasValue) {
        builder.Append("\nConversion data: ");
        builder.Append(self->conversion.value.data);
        builder.Append("\nConversion priority: ");
        builder.Append(self->conversion.value.priority);
        builder.Append("\nConversion earliest time to send: ");
        if (!self->earliestTimeToSend.hasValue) {
            builder.Append("Not set");
        } else {
            const double secondsUntilSend = self->earliestTimeToSend.value.m_value - _ZN3WTF8WallTime3nowEv().m_value;
            builder.Append(secondsUntilSend >= kOneDaySeconds && secondsUntilSend <= 2.0 * kOneDaySeconds ? "Within 24-48 hours" : "Outside 24-48 hours");
        }
        builder.Append("\nConversion request sent: ");
        builder.Append(self->conversion.value.wasSent ? "true" : "false");
    } else {
        builder.Append("\nNo conversion data.");
    }
    builder.Append("\n");
    return builder.Finish();
}

}
