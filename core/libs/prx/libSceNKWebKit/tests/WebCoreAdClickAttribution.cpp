#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/AdClickAttributionExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WebCoreAbi::AdClickAttribution;
using NKWebKit::WtfAbi::String;

constexpr double kWeek = 7.0 * 24.0 * 60.0 * 60.0;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "AdClickAttribution check failed at line %u\n", location.line());
        std::abort();
    }
}

AdClickAttribution Make(double timeOfAdClick) {
    AdClickAttribution attribution{};
    attribution.campaign = 3;
    attribution.timeOfAdClick = WTF::WallTime{timeOfAdClick};
    return attribution;
}

AdClickAttribution MakeConverted(std::uint32_t priority, bool wasSent) {
    AdClickAttribution attribution = Make(_ZN3WTF8WallTime3nowEv().m_value);
    attribution.conversion.hasValue = true;
    attribution.conversion.value = {12, priority, wasSent};
    return attribution;
}

double Now() {
    return _ZN3WTF8WallTime3nowEv().m_value;
}

void TestHasExpired() {
    AdClickAttribution fresh = Make(Now());
    Check(!_ZNK7WebCore18AdClickAttribution10hasExpiredEv(&fresh));
    AdClickAttribution almost = Make(Now() - kWeek + 3600.0);
    Check(!_ZNK7WebCore18AdClickAttribution10hasExpiredEv(&almost));
    AdClickAttribution old = Make(Now() - kWeek - 3600.0);
    Check(_ZNK7WebCore18AdClickAttribution10hasExpiredEv(&old));
    AdClickAttribution future = Make(Now() + kWeek);
    Check(!_ZNK7WebCore18AdClickAttribution10hasExpiredEv(&future));
    AdClickAttribution nan = Make(std::numeric_limits<double>::quiet_NaN());
    Check(!_ZNK7WebCore18AdClickAttribution10hasExpiredEv(&nan));
}

void TestMarkAsExpired() {
    AdClickAttribution attribution = MakeConverted(5, false);
    attribution.earliestTimeToSend = {true, WTF::WallTime{Now()}};
    _ZN7WebCore18AdClickAttribution13markAsExpiredEv(&attribution);
    Check(attribution.timeOfAdClick.m_value == 0.0);
    Check(_ZNK7WebCore18AdClickAttribution10hasExpiredEv(&attribution));
    Check(attribution.campaign == 3);
    Check(attribution.conversion.hasValue);
    Check(attribution.conversion.value.priority == 5);
    Check(attribution.earliestTimeToSend.hasValue);
}

void TestHasHigherPriorityThan() {
    AdClickAttribution none = Make(Now());
    AdClickAttribution otherNone = Make(Now());
    AdClickAttribution low = MakeConverted(2, false);
    AdClickAttribution high = MakeConverted(9, true);
    AdClickAttribution sameAsHigh = MakeConverted(9, false);
    Check(_ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(&none, &otherNone));
    Check(_ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(&low, &none));
    Check(!_ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(&none, &low));
    Check(_ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(&high, &low));
    Check(!_ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(&low, &high));
    Check(!_ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(&high, &sameAsHigh));
    Check(!_ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(&high, &high));
}

void TestConversionSent() {
    AdClickAttribution none = Make(Now());
    Check(!_ZNK7WebCore18AdClickAttribution17wasConversionSentEv(&none));
    _ZN7WebCore18AdClickAttribution20markConversionAsSentEv(&none);
    Check(!none.conversion.hasValue);
    Check(!none.conversion.value.wasSent);
    Check(!_ZNK7WebCore18AdClickAttribution17wasConversionSentEv(&none));

    AdClickAttribution pending = MakeConverted(4, false);
    Check(!_ZNK7WebCore18AdClickAttribution17wasConversionSentEv(&pending));
    _ZN7WebCore18AdClickAttribution20markConversionAsSentEv(&pending);
    Check(_ZNK7WebCore18AdClickAttribution17wasConversionSentEv(&pending));
    Check(pending.conversion.value.data == 12);
    Check(pending.conversion.value.priority == 4);
    _ZN7WebCore18AdClickAttribution20markConversionAsSentEv(&pending);
    Check(_ZNK7WebCore18AdClickAttribution17wasConversionSentEv(&pending));

    AdClickAttribution sent = MakeConverted(1, true);
    Check(_ZNK7WebCore18AdClickAttribution17wasConversionSentEv(&sent));
}

String Make8(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

String Make16(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

bool Is8Bit(const String& string) {
    return string.impl->hashAndFlags & 4;
}

std::u16string Units(const String& string) {
    Check(string.impl != nullptr);
    std::u16string units;
    for (unsigned i = 0; i < string.impl->length; ++i) {
        units.push_back(Is8Bit(string) ? static_cast<const unsigned char*>(string.impl->data)[i] : static_cast<const char16_t*>(string.impl->data)[i]);
    }
    return units;
}

std::u16string Widen(std::string_view text) {
    return std::u16string(text.begin(), text.end());
}

String Describe(const AdClickAttribution& attribution) {
    return _ZNK7WebCore18AdClickAttribution8toStringEv(&attribution);
}

void TestToStringWithoutConversion() {
    String source = Make8("example.com");
    String destination = Make8("webkit.org");
    AdClickAttribution attribution = Make(Now());
    attribution.campaign = 4294967295u;
    attribution.source = source.impl;
    attribution.destination = destination.impl;
    const String text = Describe(attribution);
    Check(Is8Bit(text));
    Check(Units(text) == Widen("Source: example.com\nDestination: webkit.org\nCampaign ID: 4294967295\nNo conversion data.\n"));

    AdClickAttribution empty{};
    const String emptyText = Describe(empty);
    Check(Is8Bit(emptyText));
    Check(Units(emptyText) == Widen("Source: \nDestination: \nCampaign ID: 0\nNo conversion data.\n"));
}

void TestToStringWithConversion() {
    String source = Make8("a.test");
    String destination = Make8("b.test");
    AdClickAttribution attribution = MakeConverted(63, false);
    attribution.source = source.impl;
    attribution.destination = destination.impl;
    const std::string_view head = "Source: a.test\nDestination: b.test\nCampaign ID: 3\nConversion data: 12\nConversion priority: 63\nConversion earliest time to send: ";

    Check(Units(Describe(attribution)) == Widen(std::string(head) + "Not set\nConversion request sent: false\n"));

    attribution.conversion.value.wasSent = true;
    attribution.earliestTimeToSend = {true, WTF::WallTime{Now() + 36.0 * 3600.0}};
    Check(Units(Describe(attribution)) == Widen(std::string(head) + "Within 24-48 hours\nConversion request sent: true\n"));

    attribution.earliestTimeToSend.value = WTF::WallTime{Now() + 3600.0};
    Check(Units(Describe(attribution)) == Widen(std::string(head) + "Outside 24-48 hours\nConversion request sent: true\n"));

    attribution.earliestTimeToSend.value = WTF::WallTime{Now() + 72.0 * 3600.0};
    Check(Units(Describe(attribution)) == Widen(std::string(head) + "Outside 24-48 hours\nConversion request sent: true\n"));

    attribution.earliestTimeToSend.value = WTF::WallTime{Now() - 36.0 * 3600.0};
    Check(Units(Describe(attribution)) == Widen(std::string(head) + "Outside 24-48 hours\nConversion request sent: true\n"));

    attribution.earliestTimeToSend.value = WTF::WallTime{std::numeric_limits<double>::quiet_NaN()};
    Check(Units(Describe(attribution)) == Widen(std::string(head) + "Outside 24-48 hours\nConversion request sent: true\n"));
}

void TestToStringWidth() {
    String latin1 = Make16(u"\u00e9");
    String wide = Make16(u"\u0100x");
    String wideLatin1 = Make16(u"ab");
    String narrow = Make8("n");

    AdClickAttribution attribution = Make(Now());
    attribution.source = latin1.impl;
    attribution.destination = narrow.impl;
    const String single = Describe(attribution);
    Check(Is8Bit(single));
    Check(Units(single) == u"Source: \u00e9\nDestination: n\nCampaign ID: 3\nNo conversion data.\n");

    attribution.source = narrow.impl;
    attribution.destination = wideLatin1.impl;
    const String upconverted = Describe(attribution);
    Check(!Is8Bit(upconverted));
    Check(Units(upconverted) == u"Source: n\nDestination: ab\nCampaign ID: 3\nNo conversion data.\n");

    attribution.source = wide.impl;
    const String wideText = Describe(attribution);
    Check(!Is8Bit(wideText));
    Check(Units(wideText) == u"Source: \u0100x\nDestination: ab\nCampaign ID: 3\nNo conversion data.\n");
}

void TestToStringNull() {
    bool thrown = false;
    try {
        _ZNK7WebCore18AdClickAttribution8toStringEv(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Check(thrown);
}

}  // namespace

int main() {
    TestHasExpired();
    TestMarkAsExpired();
    TestHasHigherPriorityThan();
    TestConversionSent();
    TestToStringWithoutConversion();
    TestToStringWithConversion();
    TestToStringWidth();
    TestToStringNull();
    std::puts("AdClickAttribution tests passed");
    return 0;
}
