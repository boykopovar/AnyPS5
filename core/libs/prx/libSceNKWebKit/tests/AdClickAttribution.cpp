#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::AdClickAbi::AdClickAttribution;
using NKWebKit::AdClickAbi::Conversion;
using NKWebKit::AdClickAbi::ExpectedConversion;
using NKWebKit::AdClickAbi::OptionalSeconds;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

const auto ParseConversionRequest = &_ZN7WebCore18AdClickAttribution22parseConversionRequestERKN3WTF3URLE;
const auto Convert = &_ZN7WebCore18AdClickAttribution31convertAndGetEarliestTimeToSendEONS0_10ConversionE;
const auto HasHigherPriorityThan = &_ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_;
const auto Url = &_ZNK7WebCore18AdClickAttribution3urlEv;
const auto UrlForTesting = &_ZNK7WebCore18AdClickAttribution13urlForTestingERKN3WTF3URLE;
const auto Referrer = &_ZNK7WebCore18AdClickAttribution8referrerEv;
const auto MarkAsExpired = &_ZN7WebCore18AdClickAttribution13markAsExpiredEv;
const auto HasExpired = &_ZNK7WebCore18AdClickAttribution10hasExpiredEv;
const auto MarkConversionAsSent = &_ZN7WebCore18AdClickAttribution20markConversionAsSentEv;
const auto WasConversionSent = &_ZNK7WebCore18AdClickAttribution17wasConversionSentEv;
const auto ToString = &_ZNK7WebCore18AdClickAttribution8toStringEv;

constexpr double Day = 24.0 * 60.0 * 60.0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "AdClickAttribution check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction> bool Throws(TAction action) {
    try {
        action();
    } catch (const TException&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

struct Text {
    std::string narrow;
    std::u16string wide;
    StringImpl impl;

    Text(const std::string& text) : narrow(text), impl{2, static_cast<unsigned>(narrow.size()), narrow.data(), 4} {}
    Text(const std::u16string& text) : wide(text), impl{2, static_cast<unsigned>(wide.size()), wide.data(), 0} {}
    Text(const Text&) = delete;
};

std::u16string Units(const StringImpl* impl) {
    std::u16string text;
    if (!impl) return text;
    for (unsigned index = 0; index < impl->length; ++index) {
        text.push_back(impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(impl->data)[index] : static_cast<const char16_t*>(impl->data)[index]);
    }
    return text;
}

std::u16string Widen(const std::string& text) {
    std::u16string wide;
    for (const unsigned char unit : text) wide.push_back(unit);
    return wide;
}

bool Is(const StringImpl* impl, const std::string& expected) {
    return impl && Units(impl) == Widen(expected);
}

struct OwnedURL {
    URL value;
    OwnedURL(URL url) : value(url) {}
    OwnedURL(const OwnedURL&) = delete;
    ~OwnedURL() { NKWebKit::WtfAbi::Release(value.impl); }
};

bool IsNullURL(const URL& url) {
    const URL null{};
    return !std::memcmp(&url, &null, sizeof(URL));
}

AdClickAttribution Make(std::uint32_t campaign, StringImpl* source, StringImpl* destination) {
    AdClickAttribution attribution{};
    attribution.campaignId = campaign;
    attribution.source = source;
    attribution.destination = destination;
    attribution.timeOfAdClick = _ZN3WTF8WallTime3nowEv().m_value;
    return attribution;
}

void TestConversion() {
    Text source(std::string("source.example"));
    Text destination(std::string("destination.example"));
    AdClickAttribution attribution = Make(12, &source.impl, &destination.impl);
    Require(!WasConversionSent(&attribution));
    MarkConversionAsSent(&attribution);
    Require(!attribution.conversion.engaged && !WasConversionSent(&attribution));

    OptionalSeconds result;
    std::memset(&result, 0xA5, sizeof(result));
    const Conversion tooMuchData{64, 0, false};
    Require(Convert(&result, &attribution, &tooMuchData) == &result);
    Require(!result.engaged && !attribution.conversion.engaged && !attribution.earliestTimeToSend.engaged);
    const unsigned char zeros[16] = {};
    Require(!std::memcmp(&result, zeros, sizeof(result)));
    const Conversion tooMuchPriority{0, 64, false};
    Convert(&result, &attribution, &tooMuchPriority);
    Require(!result.engaged && !attribution.conversion.engaged);

    const double before = _ZN3WTF8WallTime3nowEv().m_value;
    const Conversion first{63, 5, false};
    Convert(&result, &attribution, &first);
    const double after = _ZN3WTF8WallTime3nowEv().m_value;
    Require(result.engaged && result.value >= Day && result.value < 2.0 * Day);
    Require(attribution.conversion.engaged && attribution.conversion.value.data == 63 && attribution.conversion.value.priority == 5 && !attribution.conversion.value.wasSent);
    Require(attribution.earliestTimeToSend.engaged);
    Require(attribution.earliestTimeToSend.value >= before + result.value && attribution.earliestTimeToSend.value <= after + result.value);

    const double kept = attribution.earliestTimeToSend.value;
    const Conversion samePriority{1, 5, true};
    Convert(&result, &attribution, &samePriority);
    Require(!result.engaged && attribution.conversion.value.data == 63 && attribution.earliestTimeToSend.value == kept);
    const Conversion lowerPriority{1, 4, true};
    Convert(&result, &attribution, &lowerPriority);
    Require(!result.engaged && attribution.conversion.value.data == 63);

    const Conversion higherPriority{2, 6, true};
    Convert(&result, &attribution, &higherPriority);
    Require(result.engaged && attribution.conversion.value.data == 2 && attribution.conversion.value.priority == 6 && attribution.conversion.value.wasSent);
    Require(WasConversionSent(&attribution));

    attribution.conversion.value.wasSent = false;
    Require(!WasConversionSent(&attribution));
    MarkConversionAsSent(&attribution);
    Require(WasConversionSent(&attribution));

    bool varied = false;
    double previous = -1.0;
    for (std::uint32_t priority = 7; priority < 39; ++priority) {
        const Conversion next{0, priority, false};
        Convert(&result, &attribution, &next);
        Require(result.engaged && result.value >= Day && result.value < 2.0 * Day);
        if (previous >= 0.0 && previous != result.value) varied = true;
        previous = result.value;
    }
    Require(varied);

    Require(Throws<std::invalid_argument>([&] { Convert(nullptr, &attribution, &first); }));
    Require(Throws<std::invalid_argument>([&] { Convert(&result, nullptr, &first); }));
    Require(Throws<std::invalid_argument>([&] { Convert(&result, &attribution, nullptr); }));
    Require(Throws<std::invalid_argument>([] { MarkConversionAsSent(nullptr); }));
    Require(Throws<std::invalid_argument>([] { WasConversionSent(nullptr); }));
}

void TestPriority() {
    AdClickAttribution a{};
    AdClickAttribution b{};
    Require(HasHigherPriorityThan(&a, &b));
    b.conversion = {true, {0, 3, false}};
    Require(!HasHigherPriorityThan(&a, &b));
    Require(HasHigherPriorityThan(&b, &a));
    a.conversion = {true, {0, 3, false}};
    Require(!HasHigherPriorityThan(&a, &b));
    a.conversion.value.priority = 4;
    Require(HasHigherPriorityThan(&a, &b));
    Require(!HasHigherPriorityThan(&b, &a));
    Require(Throws<std::invalid_argument>([&] { HasHigherPriorityThan(nullptr, &b); }));
    Require(Throws<std::invalid_argument>([&] { HasHigherPriorityThan(&a, nullptr); }));
}

void TestExpiry() {
    AdClickAttribution attribution{};
    const double now = _ZN3WTF8WallTime3nowEv().m_value;
    attribution.timeOfAdClick = now;
    Require(!HasExpired(&attribution));
    attribution.timeOfAdClick = now - 6.0 * Day;
    Require(!HasExpired(&attribution));
    attribution.timeOfAdClick = now - 8.0 * Day;
    Require(HasExpired(&attribution));
    attribution.timeOfAdClick = now;
    MarkAsExpired(&attribution);
    Require(attribution.timeOfAdClick == 0.0 && HasExpired(&attribution));
    Require(Throws<std::invalid_argument>([] { MarkAsExpired(nullptr); }));
    Require(Throws<std::invalid_argument>([] { HasExpired(nullptr); }));
}

void TestUrls() {
    Text source(std::string("source.example"));
    Text destination(std::string("destination.example"));
    AdClickAttribution attribution = Make(12, &source.impl, &destination.impl);
    Require(IsNullURL(Url(&attribution)) && IsNullURL(Referrer(&attribution)));

    attribution.conversion = {true, {7, 3, false}};
    Require(IsNullURL(Url(&attribution)) && IsNullURL(Referrer(&attribution)));
    attribution.earliestTimeToSend = {true, 1.0};
    {
        const OwnedURL url(Url(&attribution));
        Require((url.value.bits & 1) && Is(url.value.impl, "https://source.example/.well-known/ad-click-attribution/7/12"));
        const OwnedURL referrer(Referrer(&attribution));
        Require((referrer.value.bits & 1) && Is(referrer.value.impl, "https://destination.example/"));
    }

    attribution.campaignId = 64;
    Require(IsNullURL(Url(&attribution)) && IsNullURL(Referrer(&attribution)));
    attribution.campaignId = 63;
    attribution.conversion.value.data = 64;
    Require(IsNullURL(Url(&attribution)));
    attribution.conversion.value.data = 63;
    attribution.conversion.value.priority = 64;
    Require(IsNullURL(Referrer(&attribution)));
    attribution.conversion.value.priority = 63;
    {
        const OwnedURL url(Url(&attribution));
        Require(Is(url.value.impl, "https://source.example/.well-known/ad-click-attribution/63/63"));
    }

    Text nullOrigin(std::string("nullOrigin"));
    Text empty(std::string(""));
    for (StringImpl* domain : {static_cast<StringImpl*>(nullptr), &empty.impl, &nullOrigin.impl}) {
        attribution.source = domain;
        Require(IsNullURL(Url(&attribution)) && IsNullURL(Referrer(&attribution)));
        attribution.source = &source.impl;
        attribution.destination = domain;
        Require(IsNullURL(Url(&attribution)) && IsNullURL(Referrer(&attribution)));
        attribution.destination = &destination.impl;
    }

    Text unparsable(std::string("bad host:x"));
    attribution.source = &unparsable.impl;
    Require(IsNullURL(Url(&attribution)));
    {
        const OwnedURL referrer(Referrer(&attribution));
        Require(Is(referrer.value.impl, "https://destination.example/"));
    }
    attribution.source = &source.impl;
    attribution.destination = &unparsable.impl;
    Require(IsNullURL(Referrer(&attribution)));

    Text wideSource(std::u16string(u"source.example"));
    attribution.source = &wideSource.impl;
    attribution.destination = &destination.impl;
    {
        const OwnedURL url(Url(&attribution));
        Require((url.value.bits & 1) && Is(url.value.impl, "https://source.example/.well-known/ad-click-attribution/63/63"));
    }
    Require(Throws<std::invalid_argument>([] { Url(nullptr); }));
    Require(Throws<std::invalid_argument>([] { Referrer(nullptr); }));
}

URL ParseBase(Text& input) {
    String string(&input.impl);
    const URL empty{};
    URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &empty, &string, nullptr);
    string.impl = nullptr;
    return url;
}

void TestUrlForTesting() {
    Text localhost(std::string("localhost"));
    Text loopback(std::string("127.0.0.1"));
    Text other(std::string("source.example"));
    Text destination(std::string("destination.example"));
    Text plainText(std::string("http://127.0.0.1:8000/report"));
    Text withQueryText(std::string("http://127.0.0.1:8000/report?a=b&c"));
    Text withEmptyQueryText(std::string("http://127.0.0.1:8000/report?"));
    const OwnedURL plain(ParseBase(plainText));
    const OwnedURL withQuery(ParseBase(withQueryText));
    const OwnedURL withEmptyQuery(ParseBase(withEmptyQueryText));
    Require((plain.value.bits & 1) && (withQuery.value.bits & 1) && (withEmptyQuery.value.bits & 1));

    AdClickAttribution attribution = Make(9, &other.impl, &destination.impl);
    Require(IsNullURL(UrlForTesting(&attribution, &plain.value)));
    attribution.source = nullptr;
    Require(IsNullURL(UrlForTesting(&attribution, &plain.value)));

    attribution.source = &localhost.impl;
    Require(Throws<std::logic_error>([&] { UrlForTesting(&attribution, &plain.value); }));
    attribution.conversion = {true, {5, 1, false}};
    {
        const OwnedURL url(UrlForTesting(&attribution, &plain.value));
        Require((url.value.bits & 1) && Is(url.value.impl, "http://127.0.0.1:8000/report?conversion=5&campaign=9"));
    }
    attribution.source = &loopback.impl;
    {
        const OwnedURL url(UrlForTesting(&attribution, &withQuery.value));
        Require((url.value.bits & 1) && Is(url.value.impl, "http://127.0.0.1:8000/report?conversion=5&campaign=9&a=b&c"));
    }
    {
        const OwnedURL url(UrlForTesting(&attribution, &withEmptyQuery.value));
        Require((url.value.bits & 1) && Is(url.value.impl, "http://127.0.0.1:8000/report?conversion=5&campaign=9&"));
    }
    attribution.campaignId = 4000000000u;
    attribution.conversion.value.data = 4294967295u;
    {
        const OwnedURL url(UrlForTesting(&attribution, &plain.value));
        Require(Is(url.value.impl, "http://127.0.0.1:8000/report?conversion=4294967295&campaign=4000000000"));
    }
    Require(Throws<std::invalid_argument>([&] { UrlForTesting(nullptr, &plain.value); }));
    Require(Throws<std::invalid_argument>([&] { UrlForTesting(&attribution, nullptr); }));
}

struct Parsed {
    bool has;
    std::uint32_t data;
    std::uint32_t priority;
    bool clean;
};

Parsed ParseRequest(const URL& url) {
    ExpectedConversion result;
    std::memset(&result, 0xA5, sizeof(result));
    Require(ParseConversionRequest(&result, &url) == &result);
    if (result.has) return {true, result.value.data, result.value.priority, !result.value.wasSent};
    const unsigned char zeros[sizeof(ExpectedConversion)] = {};
    return {false, 0, 0, !std::memcmp(&result, zeros, sizeof(result))};
}

Parsed ParseText(const std::string& text) {
    Text input(text);
    const OwnedURL url(ParseBase(input));
    return ParseRequest(url.value);
}

bool Accepted(const std::string& text, std::uint32_t data, std::uint32_t priority) {
    const Parsed parsed = ParseText(text);
    return parsed.has && parsed.clean && parsed.data == data && parsed.priority == priority;
}

bool Rejected(const std::string& text) {
    const Parsed parsed = ParseText(text);
    return !parsed.has && parsed.clean;
}

void TestParseConversionRequest() {
    const std::string prefix = "https://example.com/.well-known/ad-click-attribution/";
    Require(Accepted(prefix + "12", 12, 0));
    Require(Accepted(prefix + "00", 0, 0));
    Require(Accepted(prefix + "63", 63, 0));
    Require(Accepted(prefix + "12/34", 12, 34));
    Require(Accepted(prefix + "63/63", 63, 63));
    Require(Accepted(prefix + "12x34", 12, 34));
    Require(Accepted(prefix + "+5", 5, 0));
    Require(Accepted(prefix + "+5/+6", 5, 6));
    Require(Accepted("HTTPS://EXAMPLE.com/.well-known/ad-click-attribution/07", 7, 0));
    Require(Accepted("https://example.com:8443/.well-known/ad-click-attribution/07/08", 7, 8));

    Require(Rejected(prefix + "64"));
    Require(Rejected(prefix + "99"));
    Require(Rejected(prefix + "12/64"));
    Require(Rejected(prefix + "64/12"));
    Require(Rejected(prefix + "1"));
    Require(Rejected(prefix + "123"));
    Require(Rejected(prefix + "12/3"));
    Require(Rejected(prefix + "12/345"));
    Require(Rejected(prefix + "5a"));
    Require(Rejected(prefix + "a5"));
    Require(Rejected(prefix + "-5"));
    Require(Rejected(prefix + "12/-5"));
    Require(Rejected(prefix + "12/a5"));
    Require(Rejected(prefix));
    Require(Rejected("https://example.com/"));
    Require(Rejected("https://example.com/.well-known/ad-click-attributio/12"));
    Require(Rejected("https://example.com/.Well-Known/ad-click-attribution/12"));
    Require(Rejected("https://example.com/x/.well-known/ad-click-attribution/12"));
    Require(Rejected("http://example.com/.well-known/ad-click-attribution/12"));
    Require(Rejected("httpss://example.com/.well-known/ad-click-attribution/12"));
    Require(Rejected("https://user@example.com/.well-known/ad-click-attribution/12"));
    Require(Rejected("https://user:pass@example.com/.well-known/ad-click-attribution/12"));
    Require(Rejected(prefix + "12?a"));
    Require(Rejected(prefix + "12#a"));

    const URL null{};
    const Parsed fromNull = ParseRequest(null);
    Require(!fromNull.has && fromNull.clean);

    Text asciiText(prefix + "x5");
    const OwnedURL ascii(ParseBase(asciiText));
    Require(ascii.value.bits & 1);
    Require(!ParseRequest(ascii.value).has);
    std::u16string wideUnits = Widen(prefix + "x5");
    wideUnits[wideUnits.size() - 2] = 0x2028;
    Text wideText(wideUnits);
    URL wide = ascii.value;
    wide.impl = &wideText.impl;
    const Parsed spaced = ParseRequest(wide);
    Require(spaced.has && spaced.data == 5 && spaced.priority == 0);
    wideUnits[wideUnits.size() - 2] = 0x200B;
    Text zeroWidthText(wideUnits);
    wide.impl = &zeroWidthText.impl;
    Require(!ParseRequest(wide).has);
    wideUnits[wideUnits.size() - 2] = u'5';
    wideUnits[wideUnits.size() - 1] = 0x3000;
    Text trailingText(wideUnits);
    wide.impl = &trailingText.impl;
    const Parsed trailing = ParseRequest(wide);
    Require(trailing.has && trailing.data == 5);

    ExpectedConversion result;
    Require(Throws<std::invalid_argument>([&] { ParseConversionRequest(nullptr, &null); }));
    Require(Throws<std::invalid_argument>([&] { ParseConversionRequest(&result, nullptr); }));
}

void TestToString() {
    Text source(std::string("source.example"));
    Text destination(std::string("destination.example"));
    AdClickAttribution attribution = Make(12, &source.impl, &destination.impl);
    {
        const String text = ToString(&attribution);
        Require(Is(text.impl, "Source: source.example\nDestination: destination.example\nCampaign ID: 12\nNo conversion data.\n"));
        Require(text.impl->hashAndFlags & 4);
    }
    attribution.conversion = {true, {7, 3, false}};
    {
        const String text = ToString(&attribution);
        Require(Is(text.impl, "Source: source.example\nDestination: destination.example\nCampaign ID: 12\nConversion data: 7\nConversion priority: 3\n"
                              "Conversion earliest time to send: Not set\nConversion request sent: false\n"));
    }
    const double now = _ZN3WTF8WallTime3nowEv().m_value;
    attribution.earliestTimeToSend = {true, now + 1.5 * Day};
    attribution.conversion.value.wasSent = true;
    {
        const String text = ToString(&attribution);
        Require(Is(text.impl, "Source: source.example\nDestination: destination.example\nCampaign ID: 12\nConversion data: 7\nConversion priority: 3\n"
                              "Conversion earliest time to send: Within 24-48 hours\nConversion request sent: true\n"));
    }
    for (const double offset : {0.5 * Day, 2.5 * Day, -1.0 * Day}) {
        attribution.earliestTimeToSend.value = now + offset;
        const String text = ToString(&attribution);
        Require(Units(text.impl).find(u"Conversion earliest time to send: Outside 24-48 hours\n") != std::u16string::npos);
    }

    Text wideSource(std::u16string(u"s\u00e9\u4e2d"));
    AdClickAttribution wide = Make(0, &wideSource.impl, nullptr);
    {
        const String text = ToString(&wide);
        Require(Units(text.impl) == u"Source: s\u00e9\u4e2d\nDestination: \nCampaign ID: 0\nNo conversion data.\n");
        Require(!(text.impl->hashAndFlags & 4));
    }
    AdClickAttribution empty{};
    {
        const String text = ToString(&empty);
        Require(Is(text.impl, "Source: \nDestination: \nCampaign ID: 0\nNo conversion data.\n"));
    }
    Require(Throws<std::invalid_argument>([] { ToString(nullptr); }));
}

}

int main() {
    TestConversion();
    TestPriority();
    TestExpiry();
    TestUrls();
    TestUrlForTesting();
    TestParseConversionRequest();
    TestToString();
    return 0;
}
