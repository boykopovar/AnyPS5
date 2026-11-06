#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "DiagnosticLoggingKeys test failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

void RequireString(const NKWebKit::WtfAbi::String& str, std::string_view expected) {
    Require(str.impl != nullptr);
    Require((str.impl->hashAndFlags & 4) != 0);
    Require(str.impl->length == expected.length());
    const auto* data = static_cast<const char*>(str.impl->data);
    Require(std::memcmp(data, expected.data(), expected.length()) == 0);
}

void TestKeyConstants() {
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys10webViewKeyEv(), "webView");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys11cpuUsageKeyEv(), "cpuUsage");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys11inactiveKeyEv(), "inactive");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys11occurredKeyEv(), "occurred");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys11timedOutKeyEv(), "timedOut");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys12retrievalKeyEv(), "retrieval");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys13webGLStateKeyEv(), "webGLState");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys14streamingMediaEv(), "streamingMedia");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys15isAttachmentKeyEv(), "isAttachment");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys15networkCacheKeyEv(), "networkCache");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys15revalidatingKeyEv(), "revalidating");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys16internalErrorKeyEv(), "internalError");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys16notHTTPFamilyKeyEv(), "notHTTPFamily");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys17userZoomActionKeyEv(), "userZoomAction");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys18neverSeenBeforeKeyEv(), "neverSeenBefore");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys18noLongerInCacheKeyEv(), "noLongerInCache");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys18nonVisibleStateKeyEv(), "nonVisibleState");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys18unlikelyToReuseKeyEv(), "unlikelyToReuse");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys19invalidSessionIDKeyEv(), "invalidSessionID");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys19retrievalRequestKeyEv(), "retrievalRequest");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys20needsRevalidationKeyEv(), "needsRevalidation");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys20telemetryPageLoadKeyEv(), "telemetryPageLoad");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys21domainCausingCrashKeyEv(), "DomainCausingCrash");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys21simulatedPageCrashKeyEv(), "SimulatedPageCrash");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys22backNavigationDeltaKeyEv(), "backNavigationDelta");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys22cacheControlNoStoreKeyEv(), "cacheControlNoStore");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys22domainCausingJetsamKeyEv(), "DomainCausingJetsam");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys22unknownEntryRequestKeyEv(), "unknownEntryRequest");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys23isConditionalRequestKeyEv(), "isConditionalRequest");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys24activeInForegroundTabKeyEv(), "activeInForegroundTab");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys24networkProcessCrashedKeyEv(), "networkProcessCrashed");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys24uncacheableStatusCodeKeyEv(), "uncacheableStatusCode");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys24unsupportedHTTPMethodKeyEv(), "unsupportedHTTPMethod");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys24varyingHeaderMismatchKeyEv(), "varyingHeaderMismatch");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys24visibleAndActiveStateKeyEv(), "visibleAndActiveState");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys24visibleNonActiveStateKeyEv(), "visibleNonActiveState");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys25failedLessThan2SecondsKeyEv(), "failedLessThan2Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys25failedLessThan5SecondsKeyEv(), "failedLessThan5Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys25missingValidatorFieldsKeyEv(), "missingValidatorFields");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys26entryRightlyNotWarmedUpKeyEv(), "entryRightlyNotWarmedUp");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys26entryWronglyNotWarmedUpKeyEv(), "entryWronglyNotWarmedUp");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys26failedLessThan20SecondsKeyEv(), "failedLessThan20Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys26failedMoreThan20SecondsKeyEv(), "failedMoreThan20Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys27canceledLessThan2SecondsKeyEv(), "canceledLessThan2Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys27canceledLessThan5SecondsKeyEv(), "canceledLessThan5Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys27createSharedBufferFailedKeyEv(), "createSharedBufferFailed");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys27networkCacheReuseFailureKeyEv(), "networkCacheReuseFailure");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys27networkCacheUnusedReasonKeyEv(), "networkCacheUnusedReason");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys27synchronousMessageFailedKeyEv(), "synchronousMessageFailed");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys28activeInBackgroundTabOnlyKeyEv(), "activeInBackgroundTabOnly");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys28canceledLessThan20SecondsKeyEv(), "canceledLessThan20Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys28canceledMoreThan20SecondsKeyEv(), "canceledMoreThan20Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys28exceededActiveMemoryLimitKeyEv(), "ExceededActiveMemoryLimit");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys28isReloadIgnoringCacheDataKeyEv(), "isReloadIgnoringCacheData");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys28networkCacheFailureReasonKeyEv(), "networkCacheFailureReason");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys28succeededLessThan2SecondsKeyEv(), "succeededLessThan2Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys28succeededLessThan5SecondsKeyEv(), "succeededLessThan5Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys29exceededBackgroundCPULimitKeyEv(), "ExceededBackgroundCPULimit");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys29succeededLessThan20SecondsKeyEv(), "succeededLessThan20Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys29succeededMoreThan20SecondsKeyEv(), "succeededMoreThan20Seconds");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys30exceededInactiveMemoryLimitKeyEv(), "ExceededInactiveMemoryLimit");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys42wastedSpeculativeWarmupWithRevalidationKeyEv(), "wastedSpeculativeWarmupWithRevalidation");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys45wastedSpeculativeWarmupWithoutRevalidationKeyEv(), "wastedSpeculativeWarmupWithoutRevalidation");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys46successfulSpeculativeWarmupWithRevalidationKeyEv(), "successfulSpeculativeWarmupWithRevalidation");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys49successfulSpeculativeWarmupWithoutRevalidationKeyEv(), "successfulSpeculativeWarmupWithoutRevalidation");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys8otherKeyEv(), "other");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys9unusedKeyEv(), "unused");
}

void TestMemoryUsageBuckets() {
    constexpr std::uint64_t kMB = 1024ULL * 1024ULL;
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(0), "below32");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(1), "below32");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(32ULL * kMB - 1), "below32");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(32ULL * kMB), "32to64");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(64ULL * kMB - 1), "32to64");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(64ULL * kMB), "64to128");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(128ULL * kMB - 1), "64to128");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(128ULL * kMB), "128to256");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(256ULL * kMB - 1), "128to256");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(256ULL * kMB), "256to512");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(512ULL * kMB - 1), "256to512");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(512ULL * kMB), "512to1024");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(1024ULL * kMB - 1), "512to1024");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(1024ULL * kMB), "1024to2048");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(2048ULL * kMB - 1), "1024to2048");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(2048ULL * kMB), "2048to4096");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(4096ULL * kMB - 1), "2048to4096");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(4096ULL * kMB), "4096to8192");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(8192ULL * kMB - 1), "4096to8192");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(8192ULL * kMB), "8192to16384");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(16384ULL * kMB - 1), "8192to16384");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(16384ULL * kMB), "16384to32768");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(32768ULL * kMB - 1), "16384to32768");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(32768ULL * kMB), "over32768");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(65536ULL * kMB), "over32768");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(UINT64_MAX), "over32768");
}

void TestBackgroundCPUBuckets() {
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(-1.0), "below1");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(0.0), "below1");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(0.99), "below1");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(1.0), "1to5");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(4.99), "1to5");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(5.0), "5to10");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(9.99), "5to10");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(10.0), "10to30");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(29.99), "10to30");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(30.0), "30to50");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(49.99), "30to50");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(50.0), "50to70");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(69.99), "50to70");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(70.0), "over70");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(100.0), "over70");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(1000.0), "over70");
}

void TestForegroundCPUBuckets() {
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(-1.0), "below10");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(0.0), "below10");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(9.99), "below10");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(10.0), "10to20");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(19.99), "10to20");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(20.0), "20to40");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(39.99), "20to40");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(40.0), "40to60");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(59.99), "40to60");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(60.0), "60to80");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(79.99), "60to80");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(80.0), "over80");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(100.0), "over80");
    RequireString(_ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(500.0), "over80");
}

void TestRoundTripAndLifetime() {
    auto key1 = _ZN7WebCore21DiagnosticLoggingKeys10webViewKeyEv();
    Require(key1.impl != nullptr);
    Require(key1.impl->refCount == 2);
    {
        auto copy = key1;
        Require(key1.impl->refCount == 4);
        Require(copy.impl == key1.impl);
    }
    Require(key1.impl->refCount == 2);

    auto moved = std::move(key1);
    Require(key1.impl == nullptr);
    Require(moved.impl != nullptr);
    Require(moved.impl->refCount == 2);
    RequireString(moved, "webView");

    auto key2 = _ZN7WebCore21DiagnosticLoggingKeys10webViewKeyEv();
    Require(key2.impl != nullptr);
    Require(key2.impl != moved.impl);
    RequireString(key2, "webView");
}

}  // namespace

int main() {
    TestKeyConstants();
    TestMemoryUsageBuckets();
    TestBackgroundCPUBuckets();
    TestForegroundCPUBuckets();
    TestRoundTripAndLifetime();
    return 0;
}
