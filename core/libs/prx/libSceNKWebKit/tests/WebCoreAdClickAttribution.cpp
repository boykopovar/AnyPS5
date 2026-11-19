#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/AdClickAttributionExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>

namespace {

using NKWebKit::WebCoreAbi::AdClickAttribution;

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

}  // namespace

int main() {
    TestHasExpired();
    TestMarkAsExpired();
    TestHasHigherPriorityThan();
    TestConversionSent();
    std::puts("AdClickAttribution tests passed");
    return 0;
}
