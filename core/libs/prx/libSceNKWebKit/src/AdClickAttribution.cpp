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
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using NKWebKit::AdClickAbi::AdClickAttribution;
using NKWebKit::AdClickAbi::Conversion;
using NKWebKit::AdClickAbi::ExpectedConversion;
using NKWebKit::AdClickAbi::OptionalSeconds;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

static_assert(std::is_trivially_copyable_v<AdClickAttribution>);
static_assert(std::is_trivially_copyable_v<Conversion>);
static_assert(std::is_trivially_copyable_v<OptionalSeconds>);
static_assert(std::is_trivially_copyable_v<ExpectedConversion>);
static_assert(std::is_trivially_copyable_v<URL>);
static_assert(std::is_trivially_copyable_v<StringImpl>);

constexpr std::uint32_t MaxEntropy = 63;
constexpr double SecondsPerDay = 24.0 * 60.0 * 60.0;
constexpr double MaxAge = SecondsPerDay * 7.0;
constexpr char PathPrefix[] = "/.well-known/ad-click-attribution/";

template <typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename T>
void RequireWritableGuestValue(T* pointer, const char* function) {
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, sizeof(T));
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, sizeof(T));
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template <typename T>
bool TryPublishGuestValue(T* pointer, const T& value, const T& previous) {
    if (GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T))) return true;
    static_cast<void>(GuestCopyOut_nid_no_patch(pointer, &previous, sizeof(T)));
    return false;
}

template <typename T>
struct GuestRollbackGuard {
    T* target;
    const T& previous;
    bool committed = false;

    ~GuestRollbackGuard() {
        if (!committed) {
            try {
                static_cast<void>(GuestCopyOut_nid_no_patch(target, &previous, sizeof(T)));
            } catch (...) {
            }
        }
    }
};

bool DomainMatchesLoopback(const StringImpl* impl, const char* function) {
    constexpr std::string_view Localhost = "localhost";
    constexpr std::string_view Loopback = "127.0.0.1";
    static_assert(Localhost.size() == Loopback.size());
    if (!impl) return false;
    const StringImpl header = ReadGuestValue(impl, function);
    if (!header.length) return false;
    if (!header.data) throw std::invalid_argument("WebCore::AdClickAttribution: string with null characters and a nonzero length");
    const bool is8Bit = (header.hashAndFlags & 4u) != 0;
    const std::size_t byteLength = static_cast<std::size_t>(header.length) * (is8Bit ? 1u : sizeof(char16_t));
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(header.data, byteLength);
    if (!GuestReadable_nid_no_patch(header.data, byteLength))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (header.length != Localhost.size()) return false;
    char16_t units[Localhost.size()]{};
    if (is8Bit) {
        unsigned char raw[Localhost.size()]{};
        if (!GuestCopyIn_nid_no_patch(raw, header.data, sizeof(raw)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        for (std::size_t i = 0; i < Localhost.size(); ++i) units[i] = raw[i];
    } else {
        if (!GuestCopyIn_nid_no_patch(units, header.data, sizeof(units)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    const auto matches = [&](std::string_view expected) {
        for (std::size_t i = 0; i < expected.size(); ++i) {
            if (units[i] != static_cast<unsigned char>(expected[i])) return false;
        }
        return true;
    };
    return matches(Localhost) || matches(Loopback);
}

struct GuestString {
    std::vector<unsigned char> narrow;
    std::vector<char16_t> wide;
    unsigned length = 0;
    bool is8Bit = true;
    bool isNull = true;

    StringView View() const {
        if (isNull || !length) return StringView{nullptr, 0, is8Bit};
        return StringView{is8Bit ? static_cast<const void*>(narrow.data()) : static_cast<const void*>(wide.data()), length, is8Bit};
    }

    String ToWtfString() const {
        if (isNull) return String{};
        if (length > static_cast<unsigned>(std::numeric_limits<std::int32_t>::max()))
            throw std::length_error("WebCore::AdClickAttribution: string exceeds the maximum length");
        void* characters = nullptr;
        String result(NKWebKit::WtfAbi::CreateUninitialized(length, is8Bit, characters));
        if (length) {
            if (is8Bit) std::memcpy(characters, narrow.data(), length);
            else std::memcpy(characters, wide.data(), static_cast<std::size_t>(length) * sizeof(char16_t));
        }
        return result;
    }
};

GuestString ReadGuestString(const StringImpl* impl, const char* function, const char* nullCharactersMessage) {
    GuestString result{};
    if (!impl) return result;
    const StringImpl header = ReadGuestValue(impl, function);
    result.isNull = false;
    result.length = header.length;
    result.is8Bit = (header.hashAndFlags & 4u) != 0;
    if (!header.length) return result;
    if (!header.data) throw std::invalid_argument(nullCharactersMessage);
    const std::size_t byteLength = static_cast<std::size_t>(header.length) * (result.is8Bit ? 1u : sizeof(char16_t));
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(header.data, byteLength);
    if (!GuestReadable_nid_no_patch(header.data, byteLength))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (result.is8Bit) {
        result.narrow.resize(header.length);
        if (!GuestCopyIn_nid_no_patch(result.narrow.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    } else {
        result.wide.resize(header.length);
        if (!GuestCopyIn_nid_no_patch(result.wide.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    return result;
}

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

    void Append(const GuestString& value) {
        if (!value.isNull) Append(value.View());
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

bool Equals(const GuestString& value, std::string_view literal) {
    if (value.isNull) return false;
    const StringView view = value.View();
    if (view.length != literal.size()) return false;
    for (unsigned index = 0; index < view.length; ++index) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, index) != static_cast<unsigned char>(literal[index])) return false;
    }
    return true;
}

bool DomainIsEmpty(const GuestString& domain) {
    return domain.isNull || !domain.length || Equals(domain, "nullOrigin");
}

bool ConversionIsValid(const Conversion& conversion) {
    return conversion.data <= MaxEntropy && conversion.priority <= MaxEntropy;
}

struct ValidDomains {
    bool valid = false;
    GuestString source;
    GuestString destination;
};

ValidDomains ValidateAttribution(const AdClickAttribution& self, const char* function) {
    ValidDomains domains{};
    if (!self.conversion.engaged || !ConversionIsValid(self.conversion.value) || self.campaignId > MaxEntropy) return domains;
    domains.source = ReadGuestString(self.source, function, "WebCore::AdClickAttribution: string with null characters and a nonzero length");
    if (DomainIsEmpty(domains.source)) return domains;
    domains.destination = ReadGuestString(self.destination, function, "WebCore::AdClickAttribution: string with null characters and a nonzero length");
    if (DomainIsEmpty(domains.destination) || !self.earliestTimeToSend.engaged) return domains;
    domains.valid = true;
    return domains;
}

AdClickAttribution ReadChecked(const AdClickAttribution* self, const char* function) {
    if (!self) throw std::invalid_argument("WebCore::AdClickAttribution: null this");
    return ReadGuestValue(self, function);
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

GuestString WholeString(const URL& url, const char* function) {
    return ReadGuestString(url.impl, function, "WebCore::AdClickAttribution: URL string with null characters and a nonzero length");
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
    const URL url = ReadGuestValue(redirectURL, __func__);
    const ExpectedConversion previousResult = ReadGuestValue(result, __func__);
    RequireWritableGuestValue(result, __func__);
    const GuestString wholeStorage = WholeString(url, __func__);
    const StringView whole = wholeStorage.View();
    ExpectedConversion output{};
    std::memset(&output, 0, sizeof(output));
    const auto publish = [&]() -> ExpectedConversion* {
        if (!TryPublishGuestValue(result, output, previousResult))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return result;
    };
    const bool hasCredentials = url.passwordEnd > url.userStart;
    const bool hasQuery = url.queryEnd > url.pathEnd;
    const bool hasFragment = (url.bits & 1) && whole.length > url.queryEnd;
    if (!ProtocolIsHttps(url, whole) || hasCredentials || hasQuery || hasFragment) return publish();
    const unsigned pathStart = url.hostEnd + ((url.bits >> 3) & 7);
    const StringView path = Substring(whole, pathStart, url.pathEnd - pathStart);
    if (!path.length || !StartsWith(path, Prefix)) return publish();
    const unsigned prefixLength = static_cast<unsigned>(Prefix.size());
    std::uint64_t data = 0;
    if (path.length == prefixLength + DataSize) {
        if (!ToUInt64Strict(Substring(path, prefixLength, DataSize), data) || data > MaxEntropy) return publish();
        output.value = Conversion{static_cast<std::uint32_t>(data), 0, false};
        output.has = true;
        return publish();
    }
    if (path.length == prefixLength + DataSize + 1 + PrioritySize) {
        if (!ToUInt64Strict(Substring(path, prefixLength, DataSize), data) || data > MaxEntropy) return publish();
        std::uint64_t priority = 0;
        if (!ToUInt64Strict(Substring(path, prefixLength + DataSize + 1, PrioritySize), priority) || priority > MaxEntropy) return publish();
        output.value = Conversion{static_cast<std::uint32_t>(data), static_cast<std::uint32_t>(priority), false};
        output.has = true;
        return publish();
    }
    return publish();
}

OptionalSeconds* APS5_VABI _ZN7WebCore18AdClickAttribution31convertAndGetEarliestTimeToSendEONS0_10ConversionE(OptionalSeconds* result, AdClickAttribution* self, const Conversion* conversion) {
    if (!result) throw std::invalid_argument("WebCore::AdClickAttribution::convertAndGetEarliestTimeToSend: null result");
    const AdClickAttribution previousSelf = ReadChecked(self, __func__);
    if (!conversion) throw std::invalid_argument("WebCore::AdClickAttribution::convertAndGetEarliestTimeToSend: null conversion");
    const Conversion localConversion = ReadGuestValue(conversion, __func__);
    const OptionalSeconds previousResult = ReadGuestValue(result, __func__);
    RequireWritableGuestValue(result, __func__);
    RequireWritableGuestValue(self, __func__);
    OptionalSeconds outputResult{};
    std::memset(&outputResult, 0, sizeof(outputResult));
    if (!ConversionIsValid(localConversion) || (previousSelf.conversion.engaged && previousSelf.conversion.value.priority >= localConversion.priority)) {
        if (!TryPublishGuestValue(result, outputResult, previousResult))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return result;
    }
    AdClickAttribution updatedSelf = previousSelf;
    updatedSelf.conversion.value.data = localConversion.data;
    updatedSelf.conversion.value.priority = localConversion.priority;
    updatedSelf.conversion.value.wasSent = localConversion.wasSent;
    updatedSelf.conversion.engaged = true;
    const double seconds = SecondsPerDay + RandomNumber() * SecondsPerDay;
    updatedSelf.earliestTimeToSend.value = Now() + seconds;
    updatedSelf.earliestTimeToSend.engaged = true;
    outputResult.value = seconds;
    outputResult.engaged = true;
    if (!TryPublishGuestValue(self, updatedSelf, previousSelf))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    GuestRollbackGuard<AdClickAttribution> rollback{self, previousSelf};
    if (!TryPublishGuestValue(result, outputResult, previousResult))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    rollback.committed = true;
    return result;
}

bool APS5_VABI _ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(const AdClickAttribution* self, const AdClickAttribution* other) {
    const AdClickAttribution attribution = ReadChecked(self, __func__);
    if (!other) throw std::invalid_argument("WebCore::AdClickAttribution::hasHigherPriorityThan: null other");
    const AdClickAttribution otherAttribution = ReadGuestValue(other, __func__);
    if (!otherAttribution.conversion.engaged) return true;
    if (!attribution.conversion.engaged) return false;
    return attribution.conversion.value.priority > otherAttribution.conversion.value.priority;
}

URL APS5_VABI _ZNK7WebCore18AdClickAttribution3urlEv(const AdClickAttribution* self) {
    const AdClickAttribution attribution = ReadChecked(self, __func__);
    const ValidDomains domains = ValidateAttribution(attribution, __func__);
    if (!domains.valid) return URL{};
    Builder builder;
    builder.Append("https://");
    builder.Append(domains.source);
    builder.Append(PathPrefix);
    builder.AppendNumber(attribution.conversion.value.data);
    builder.Append("/");
    builder.AppendNumber(attribution.campaignId);
    return ValidOrNull(Parse(URL{}, builder.ToString()));
}

URL APS5_VABI _ZNK7WebCore18AdClickAttribution13urlForTestingERKN3WTF3URLE(const AdClickAttribution* self, const URL* baseURL) {
    const AdClickAttribution attribution = ReadChecked(self, __func__);
    if (!baseURL) throw std::invalid_argument("WebCore::AdClickAttribution::urlForTesting: null base URL");
    const URL base = ReadGuestValue(baseURL, __func__);
    if (!DomainMatchesLoopback(attribution.source, __func__)) return URL{};
    const Conversion& conversion = RequiredConversion(attribution);
    const GuestString baseWhole = WholeString(base, __func__);
    Builder builder;
    builder.Append("?conversion=");
    builder.AppendNumber(conversion.data);
    builder.Append("&campaign=");
    builder.AppendNumber(attribution.campaignId);
    if (base.queryEnd > base.pathEnd) {
        builder.Append("&");
        builder.Append(Substring(baseWhole.View(), base.pathEnd + 1, base.queryEnd - (base.pathEnd + 1)));
    }
    URL safeBase = base;
    const String ownedBase = baseWhole.ToWtfString();
    safeBase.impl = ownedBase.impl;
    return Parse(safeBase, builder.ToString());
}

URL APS5_VABI _ZNK7WebCore18AdClickAttribution8referrerEv(const AdClickAttribution* self) {
    const AdClickAttribution attribution = ReadChecked(self, __func__);
    const ValidDomains domains = ValidateAttribution(attribution, __func__);
    if (!domains.valid) return URL{};
    Builder builder;
    builder.Append("https://");
    builder.Append(domains.destination);
    builder.Append("/");
    return ValidOrNull(Parse(URL{}, builder.ToString()));
}

void APS5_VABI _ZN7WebCore18AdClickAttribution13markAsExpiredEv(AdClickAttribution* self) {
    const AdClickAttribution previous = ReadChecked(self, __func__);
    RequireWritableGuestValue(self, __func__);
    AdClickAttribution updated = previous;
    updated.timeOfAdClick = 0.0;
    if (!TryPublishGuestValue(self, updated, previous))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

bool APS5_VABI _ZNK7WebCore18AdClickAttribution10hasExpiredEv(const AdClickAttribution* self) {
    return Now() > ReadChecked(self, __func__).timeOfAdClick + MaxAge;
}

void APS5_VABI _ZN7WebCore18AdClickAttribution20markConversionAsSentEv(AdClickAttribution* self) {
    const AdClickAttribution previous = ReadChecked(self, __func__);
    RequireWritableGuestValue(self, __func__);
    if (!previous.conversion.engaged) return;
    AdClickAttribution updated = previous;
    updated.conversion.value.wasSent = true;
    if (!TryPublishGuestValue(self, updated, previous))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

bool APS5_VABI _ZNK7WebCore18AdClickAttribution17wasConversionSentEv(const AdClickAttribution* self) {
    const AdClickAttribution attribution = ReadChecked(self, __func__);
    return attribution.conversion.engaged && attribution.conversion.value.wasSent;
}

String APS5_VABI _ZNK7WebCore18AdClickAttribution8toStringEv(const AdClickAttribution* self) {
    const AdClickAttribution attribution = ReadChecked(self, __func__);
    const GuestString source = ReadGuestString(attribution.source, __func__, "WebCore::AdClickAttribution: string with null characters and a nonzero length");
    const GuestString destination = ReadGuestString(attribution.destination, __func__, "WebCore::AdClickAttribution: string with null characters and a nonzero length");
    Builder builder;
    builder.Append("Source: ");
    builder.Append(source);
    builder.Append("\nDestination: ");
    builder.Append(destination);
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
