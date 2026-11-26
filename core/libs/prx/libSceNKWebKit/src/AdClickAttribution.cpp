// Derived from Source/WebCore/loader/AdClickAttribution.cpp@c55364d53f971f63a709976f168d932d558b8909
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

#include "prx/libSceNKWebKit/include/AdClickAttributionExports.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::AdClickAbi::AdClickAttribution;
using NKWebKit::AdClickAbi::Conversion;
using NKWebKit::AdClickAbi::ExpectedConversion;
using NKWebKit::AdClickAbi::OptionalSeconds;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

constexpr std::uint32_t MaxEntropy = 63;
constexpr double SecondsPerDay = 24.0 * 60.0 * 60.0;
constexpr double MaxAge = SecondsPerDay * 7.0;
constexpr char PathPrefix[] = "/.well-known/ad-click-attribution/";

struct Builder {
    std::u16string text;
    bool is8Bit = true;

    void Append(std::string_view literal) {
        for (const char unit : literal) text.push_back(static_cast<unsigned char>(unit));
    }

    void Append(StringView view) {
        if (view.length && !view.data) throw std::invalid_argument("WebCore::AdClickAttribution: string with null characters and a nonzero length");
        is8Bit = is8Bit && view.is8Bit;
        for (unsigned index = 0; index < view.length; ++index) text.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    }

    void Append(const StringImpl* impl) {
        if (impl) Append(NKWebKit::WtfAbi::ViewOf(impl));
    }

    void AppendNumber(std::uint32_t number) {
        Append(std::to_string(number));
    }

    String ToString() const {
        if (text.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) throw std::length_error("WebCore::AdClickAttribution: string exceeds the maximum length");
        void* characters = nullptr;
        String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(text.size()), is8Bit, characters));
        for (std::size_t index = 0; index < text.size(); ++index) {
            if (is8Bit) static_cast<unsigned char*>(characters)[index] = static_cast<unsigned char>(text[index]);
            else static_cast<char16_t*>(characters)[index] = text[index];
        }
        return result;
    }
};

bool Equals(const StringImpl* impl, std::string_view literal) {
    if (!impl) return false;
    const StringView view = NKWebKit::WtfAbi::ViewOf(impl);
    if (view.length != literal.size()) return false;
    if (view.length && !view.data) throw std::invalid_argument("WebCore::AdClickAttribution: string with null characters and a nonzero length");
    for (unsigned index = 0; index < view.length; ++index) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, index) != static_cast<unsigned char>(literal[index])) return false;
    }
    return true;
}

bool DomainIsEmpty(const StringImpl* domain) {
    return !domain || !domain->length || Equals(domain, "nullOrigin");
}

bool ConversionIsValid(const Conversion& conversion) {
    return conversion.data <= MaxEntropy && conversion.priority <= MaxEntropy;
}

bool IsValid(const AdClickAttribution& self) {
    return self.conversion.engaged && ConversionIsValid(self.conversion.value) && self.campaignId <= MaxEntropy && !DomainIsEmpty(self.source) &&
           !DomainIsEmpty(self.destination) && self.earliestTimeToSend.engaged;
}

const AdClickAttribution& Checked(const AdClickAttribution* self) {
    if (!self) throw std::invalid_argument("WebCore::AdClickAttribution: null this");
    return *self;
}

AdClickAttribution& Checked(AdClickAttribution* self) {
    if (!self) throw std::invalid_argument("WebCore::AdClickAttribution: null this");
    return *self;
}

const Conversion& RequiredConversion(const AdClickAttribution& self) {
    if (!self.conversion.engaged) throw std::logic_error("WebCore::AdClickAttribution::urlForTesting: no conversion");
    return self.conversion.value;
}

URL Parse(const URL& base, const String& text) {
    URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &text, nullptr);
    return url;
}

URL ValidOrNull(URL url) {
    if (url.bits & 1) return url;
    NKWebKit::WtfAbi::Release(url.impl);
    return URL{};
}

double Now() {
    return _ZN3WTF8WallTime3nowEv().m_value;
}

double RandomNumber() {
    std::uint32_t bits = 0;
    _ZN3WTF29cryptographicallyRandomValuesEPvm(&bits, sizeof(bits));
    return static_cast<double>(bits) / (static_cast<double>(std::numeric_limits<std::uint32_t>::max()) + 1.0);
}

StringView WholeString(const URL& url) {
    if (!url.impl) return StringView{nullptr, 0, true};
    const StringView view = NKWebKit::WtfAbi::ViewOf(url.impl);
    if (view.length && !view.data) throw std::invalid_argument("WebCore::AdClickAttribution: URL string with null characters and a nonzero length");
    return view;
}

StringView Substring(const StringView& view, unsigned start, unsigned length) {
    if (start >= view.length) return StringView{nullptr, 0, true};
    const unsigned maxLength = view.length - start;
    if (length > maxLength) length = maxLength;
    const auto* bytes = static_cast<const unsigned char*>(view.data);
    return StringView{bytes + (view.is8Bit ? start : static_cast<std::size_t>(start) * 2u), length, view.is8Bit};
}

bool StartsWith(const StringView& view, std::string_view prefix) {
    if (view.length < prefix.size()) return false;
    for (unsigned index = 0; index < prefix.size(); ++index) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, index) != static_cast<unsigned char>(prefix[index])) return false;
    }
    return true;
}

bool ProtocolIsHttps(const URL& url, const StringView& whole) {
    constexpr std::string_view Protocol = "https";
    if (!(url.bits & 1)) return false;
    const unsigned schemeEnd = url.bits >> 6;
    for (unsigned index = 0; index < schemeEnd; ++index) {
        if (index >= Protocol.size() || index >= whole.length) return false;
        if ((NKWebKit::WtfAbi::CodeUnitAt(whole, index) | 0x20) != static_cast<unsigned char>(Protocol[index])) return false;
    }
    return schemeEnd == Protocol.size();
}

bool IsSpaceOrNewline(char16_t unit) {
    if (unit < 0x80) return unit == u' ' || (unit >= 0x9 && unit <= 0xD);
    return unit == 0x1680 || (unit >= 0x2000 && unit <= 0x200A) || unit == 0x2028 || unit == 0x205F || unit == 0x3000;
}

bool ToUInt64Strict(const StringView& view, std::uint64_t& value) {
    constexpr std::uint64_t Max = std::numeric_limits<std::uint64_t>::max();
    value = 0;
    if (!view.data) return false;
    unsigned index = 0;
    while (index < view.length && IsSpaceOrNewline(NKWebKit::WtfAbi::CodeUnitAt(view, index))) ++index;
    if (index < view.length && NKWebKit::WtfAbi::CodeUnitAt(view, index) == u'+') ++index;
    const auto isDigit = [&](unsigned at) {
        const char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(view, at);
        return unit >= u'0' && unit <= u'9';
    };
    if (index >= view.length || !isDigit(index)) return false;
    std::uint64_t result = 0;
    while (index < view.length && isDigit(index)) {
        const std::uint64_t digit = NKWebKit::WtfAbi::CodeUnitAt(view, index) - u'0';
        if (result > Max / 10 || (result == Max / 10 && digit > Max % 10)) return false;
        result = 10 * result + digit;
        ++index;
    }
    while (index < view.length && IsSpaceOrNewline(NKWebKit::WtfAbi::CodeUnitAt(view, index))) ++index;
    if (index != view.length) return false;
    value = result;
    return true;
}

}

extern "C" {

ExpectedConversion* APS5_VABI _ZN7WebCore18AdClickAttribution22parseConversionRequestERKN3WTF3URLE(ExpectedConversion* result, const URL* redirectURL) {
    constexpr std::string_view Prefix = PathPrefix;
    constexpr unsigned DataSize = 2;
    constexpr unsigned PrioritySize = 2;
    if (!result) throw std::invalid_argument("WebCore::AdClickAttribution::parseConversionRequest: null result");
    if (!redirectURL) throw std::invalid_argument("WebCore::AdClickAttribution::parseConversionRequest: null URL");
    std::memset(result, 0, sizeof(*result));
    const URL& url = *redirectURL;
    const StringView whole = WholeString(url);
    const bool hasCredentials = url.passwordEnd > url.userStart;
    const bool hasQuery = url.queryEnd > url.pathEnd;
    const bool hasFragment = (url.bits & 1) && whole.length > url.queryEnd;
    if (!ProtocolIsHttps(url, whole) || hasCredentials || hasQuery || hasFragment) return result;
    const unsigned pathStart = url.hostEnd + ((url.bits >> 3) & 7);
    const StringView path = Substring(whole, pathStart, url.pathEnd - pathStart);
    if (!path.length || !StartsWith(path, Prefix)) return result;
    const unsigned prefixLength = static_cast<unsigned>(Prefix.size());
    std::uint64_t data = 0;
    if (path.length == prefixLength + DataSize) {
        if (!ToUInt64Strict(Substring(path, prefixLength, DataSize), data) || data > MaxEntropy) return result;
        result->value = Conversion{static_cast<std::uint32_t>(data), 0, false};
        result->has = true;
        return result;
    }
    if (path.length == prefixLength + DataSize + 1 + PrioritySize) {
        if (!ToUInt64Strict(Substring(path, prefixLength, DataSize), data) || data > MaxEntropy) return result;
        std::uint64_t priority = 0;
        if (!ToUInt64Strict(Substring(path, prefixLength + DataSize + 1, PrioritySize), priority) || priority > MaxEntropy) return result;
        result->value = Conversion{static_cast<std::uint32_t>(data), static_cast<std::uint32_t>(priority), false};
        result->has = true;
        return result;
    }
    return result;
}

OptionalSeconds* APS5_VABI _ZN7WebCore18AdClickAttribution31convertAndGetEarliestTimeToSendEONS0_10ConversionE(OptionalSeconds* result, AdClickAttribution* self, const Conversion* conversion) {
    if (!result) throw std::invalid_argument("WebCore::AdClickAttribution::convertAndGetEarliestTimeToSend: null result");
    AdClickAttribution& attribution = Checked(self);
    if (!conversion) throw std::invalid_argument("WebCore::AdClickAttribution::convertAndGetEarliestTimeToSend: null conversion");
    std::memset(result, 0, sizeof(*result));
    if (!ConversionIsValid(*conversion) || (attribution.conversion.engaged && attribution.conversion.value.priority >= conversion->priority)) return result;
    attribution.conversion.value.data = conversion->data;
    attribution.conversion.value.priority = conversion->priority;
    attribution.conversion.value.wasSent = conversion->wasSent;
    attribution.conversion.engaged = true;
    const double seconds = SecondsPerDay + RandomNumber() * SecondsPerDay;
    attribution.earliestTimeToSend.value = Now() + seconds;
    attribution.earliestTimeToSend.engaged = true;
    result->value = seconds;
    result->engaged = true;
    return result;
}

bool APS5_VABI _ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(const AdClickAttribution* self, const AdClickAttribution* other) {
    const AdClickAttribution& attribution = Checked(self);
    if (!other) throw std::invalid_argument("WebCore::AdClickAttribution::hasHigherPriorityThan: null other");
    if (!other->conversion.engaged) return true;
    if (!attribution.conversion.engaged) return false;
    return attribution.conversion.value.priority > other->conversion.value.priority;
}

URL APS5_VABI _ZNK7WebCore18AdClickAttribution3urlEv(const AdClickAttribution* self) {
    const AdClickAttribution& attribution = Checked(self);
    if (!IsValid(attribution)) return URL{};
    Builder builder;
    builder.Append("https://");
    builder.Append(attribution.source);
    builder.Append(PathPrefix);
    builder.AppendNumber(attribution.conversion.value.data);
    builder.Append("/");
    builder.AppendNumber(attribution.campaignId);
    return ValidOrNull(Parse(URL{}, builder.ToString()));
}

URL APS5_VABI _ZNK7WebCore18AdClickAttribution13urlForTestingERKN3WTF3URLE(const AdClickAttribution* self, const URL* baseURL) {
    const AdClickAttribution& attribution = Checked(self);
    if (!baseURL) throw std::invalid_argument("WebCore::AdClickAttribution::urlForTesting: null base URL");
    if (!Equals(attribution.source, "localhost") && !Equals(attribution.source, "127.0.0.1")) return URL{};
    const Conversion& conversion = RequiredConversion(attribution);
    Builder builder;
    builder.Append("?conversion=");
    builder.AppendNumber(conversion.data);
    builder.Append("&campaign=");
    builder.AppendNumber(attribution.campaignId);
    if (baseURL->queryEnd > baseURL->pathEnd) {
        builder.Append("&");
        builder.Append(Substring(WholeString(*baseURL), baseURL->pathEnd + 1, baseURL->queryEnd - (baseURL->pathEnd + 1)));
    }
    return Parse(*baseURL, builder.ToString());
}

URL APS5_VABI _ZNK7WebCore18AdClickAttribution8referrerEv(const AdClickAttribution* self) {
    const AdClickAttribution& attribution = Checked(self);
    if (!IsValid(attribution)) return URL{};
    Builder builder;
    builder.Append("https://");
    builder.Append(attribution.destination);
    builder.Append("/");
    return ValidOrNull(Parse(URL{}, builder.ToString()));
}

void APS5_VABI _ZN7WebCore18AdClickAttribution13markAsExpiredEv(AdClickAttribution* self) {
    Checked(self).timeOfAdClick = 0.0;
}

bool APS5_VABI _ZNK7WebCore18AdClickAttribution10hasExpiredEv(const AdClickAttribution* self) {
    return Now() > Checked(self).timeOfAdClick + MaxAge;
}

void APS5_VABI _ZN7WebCore18AdClickAttribution20markConversionAsSentEv(AdClickAttribution* self) {
    AdClickAttribution& attribution = Checked(self);
    if (attribution.conversion.engaged) attribution.conversion.value.wasSent = true;
}

bool APS5_VABI _ZNK7WebCore18AdClickAttribution17wasConversionSentEv(const AdClickAttribution* self) {
    const AdClickAttribution& attribution = Checked(self);
    return attribution.conversion.engaged && attribution.conversion.value.wasSent;
}

String APS5_VABI _ZNK7WebCore18AdClickAttribution8toStringEv(const AdClickAttribution* self) {
    const AdClickAttribution& attribution = Checked(self);
    Builder builder;
    builder.Append("Source: ");
    builder.Append(attribution.source);
    builder.Append("\nDestination: ");
    builder.Append(attribution.destination);
    builder.Append("\nCampaign ID: ");
    builder.AppendNumber(attribution.campaignId);
    if (attribution.conversion.engaged) {
        builder.Append("\nConversion data: ");
        builder.AppendNumber(attribution.conversion.value.data);
        builder.Append("\nConversion priority: ");
        builder.AppendNumber(attribution.conversion.value.priority);
        builder.Append("\nConversion earliest time to send: ");
        if (!attribution.earliestTimeToSend.engaged) {
            builder.Append("Not set");
        } else {
            const double secondsUntilSend = attribution.earliestTimeToSend.value - Now();
            builder.Append(secondsUntilSend >= SecondsPerDay && secondsUntilSend <= 2.0 * SecondsPerDay ? "Within 24-48 hours" : "Outside 24-48 hours");
        }
        builder.Append("\nConversion request sent: ");
        builder.Append(attribution.conversion.value.wasSent ? "true" : "false");
    } else {
        builder.Append("\nNo conversion data.");
    }
    builder.Append("\n");
    return builder.ToString();
}

}
