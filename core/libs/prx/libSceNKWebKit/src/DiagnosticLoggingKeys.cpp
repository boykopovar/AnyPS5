/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
#include "prx/libSceNKWebKit/include/DiagnosticLoggingKeysExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "WtfStringAbi.hpp"
#include <cstdint>

namespace {

NKWebKit::WtfAbi::String MakeKey(const char* literal, unsigned length) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(literal), length);
}

template<std::size_t N>
NKWebKit::WtfAbi::String MakeKey(const char (&literal)[N]) {
    return MakeKey(literal, static_cast<unsigned>(N - 1));
}

constexpr std::uint64_t kMB = 1024ULL * 1024ULL;

}  // namespace

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys10webViewKeyEv() {
    return MakeKey("webView");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys11cpuUsageKeyEv() {
    return MakeKey("cpuUsage");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys11inactiveKeyEv() {
    return MakeKey("inactive");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys11occurredKeyEv() {
    return MakeKey("occurred");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys11timedOutKeyEv() {
    return MakeKey("timedOut");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys12retrievalKeyEv() {
    return MakeKey("retrieval");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys13webGLStateKeyEv() {
    return MakeKey("webGLState");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys14streamingMediaEv() {
    return MakeKey("streamingMedia");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys15isAttachmentKeyEv() {
    return MakeKey("isAttachment");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys15networkCacheKeyEv() {
    return MakeKey("networkCache");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys15revalidatingKeyEv() {
    return MakeKey("revalidating");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys16internalErrorKeyEv() {
    return MakeKey("internalError");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys16notHTTPFamilyKeyEv() {
    return MakeKey("notHTTPFamily");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys17userZoomActionKeyEv() {
    return MakeKey("userZoomAction");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys18neverSeenBeforeKeyEv() {
    return MakeKey("neverSeenBefore");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys18noLongerInCacheKeyEv() {
    return MakeKey("noLongerInCache");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys18nonVisibleStateKeyEv() {
    return MakeKey("nonVisibleState");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys18unlikelyToReuseKeyEv() {
    return MakeKey("unlikelyToReuse");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys19invalidSessionIDKeyEv() {
    return MakeKey("invalidSessionID");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys19retrievalRequestKeyEv() {
    return MakeKey("retrievalRequest");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys20needsRevalidationKeyEv() {
    return MakeKey("needsRevalidation");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys20telemetryPageLoadKeyEv() {
    return MakeKey("telemetryPageLoad");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys21domainCausingCrashKeyEv() {
    return MakeKey("DomainCausingCrash");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys21simulatedPageCrashKeyEv() {
    return MakeKey("SimulatedPageCrash");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys22backNavigationDeltaKeyEv() {
    return MakeKey("backNavigationDelta");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys22cacheControlNoStoreKeyEv() {
    return MakeKey("cacheControlNoStore");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys22domainCausingJetsamKeyEv() {
    return MakeKey("DomainCausingJetsam");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys22unknownEntryRequestKeyEv() {
    return MakeKey("unknownEntryRequest");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys23isConditionalRequestKeyEv() {
    return MakeKey("isConditionalRequest");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys24activeInForegroundTabKeyEv() {
    return MakeKey("activeInForegroundTab");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys24networkProcessCrashedKeyEv() {
    return MakeKey("networkProcessCrashed");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys24uncacheableStatusCodeKeyEv() {
    return MakeKey("uncacheableStatusCode");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys24unsupportedHTTPMethodKeyEv() {
    return MakeKey("unsupportedHTTPMethod");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys24varyingHeaderMismatchKeyEv() {
    return MakeKey("varyingHeaderMismatch");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys24visibleAndActiveStateKeyEv() {
    return MakeKey("visibleAndActiveState");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys24visibleNonActiveStateKeyEv() {
    return MakeKey("visibleNonActiveState");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys25failedLessThan2SecondsKeyEv() {
    return MakeKey("failedLessThan2Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys25failedLessThan5SecondsKeyEv() {
    return MakeKey("failedLessThan5Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys25missingValidatorFieldsKeyEv() {
    return MakeKey("missingValidatorFields");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys26entryRightlyNotWarmedUpKeyEv() {
    return MakeKey("entryRightlyNotWarmedUp");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys26entryWronglyNotWarmedUpKeyEv() {
    return MakeKey("entryWronglyNotWarmedUp");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys26failedLessThan20SecondsKeyEv() {
    return MakeKey("failedLessThan20Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys26failedMoreThan20SecondsKeyEv() {
    return MakeKey("failedMoreThan20Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys27canceledLessThan2SecondsKeyEv() {
    return MakeKey("canceledLessThan2Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys27canceledLessThan5SecondsKeyEv() {
    return MakeKey("canceledLessThan5Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys27createSharedBufferFailedKeyEv() {
    return MakeKey("createSharedBufferFailed");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys27networkCacheReuseFailureKeyEv() {
    return MakeKey("networkCacheReuseFailure");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys27networkCacheUnusedReasonKeyEv() {
    return MakeKey("networkCacheUnusedReason");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys27synchronousMessageFailedKeyEv() {
    return MakeKey("synchronousMessageFailed");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys28activeInBackgroundTabOnlyKeyEv() {
    return MakeKey("activeInBackgroundTabOnly");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys28canceledLessThan20SecondsKeyEv() {
    return MakeKey("canceledLessThan20Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys28canceledMoreThan20SecondsKeyEv() {
    return MakeKey("canceledMoreThan20Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys28exceededActiveMemoryLimitKeyEv() {
    return MakeKey("ExceededActiveMemoryLimit");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys28isReloadIgnoringCacheDataKeyEv() {
    return MakeKey("isReloadIgnoringCacheData");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys28networkCacheFailureReasonKeyEv() {
    return MakeKey("networkCacheFailureReason");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys28succeededLessThan2SecondsKeyEv() {
    return MakeKey("succeededLessThan2Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys28succeededLessThan5SecondsKeyEv() {
    return MakeKey("succeededLessThan5Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys29exceededBackgroundCPULimitKeyEv() {
    return MakeKey("ExceededBackgroundCPULimit");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys29succeededLessThan20SecondsKeyEv() {
    return MakeKey("succeededLessThan20Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys29succeededMoreThan20SecondsKeyEv() {
    return MakeKey("succeededMoreThan20Seconds");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys30exceededInactiveMemoryLimitKeyEv() {
    return MakeKey("ExceededInactiveMemoryLimit");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys33memoryUsageToDiagnosticLoggingKeyEm(std::uint64_t memoryUsage) {
    if (memoryUsage < 32ULL * kMB) return MakeKey("below32");
    if (memoryUsage < 64ULL * kMB) return MakeKey("32to64");
    if (memoryUsage < 128ULL * kMB) return MakeKey("64to128");
    if (memoryUsage < 256ULL * kMB) return MakeKey("128to256");
    if (memoryUsage < 512ULL * kMB) return MakeKey("256to512");
    if (memoryUsage < 1024ULL * kMB) return MakeKey("512to1024");
    if (memoryUsage < 2048ULL * kMB) return MakeKey("1024to2048");
    if (memoryUsage < 4096ULL * kMB) return MakeKey("2048to4096");
    if (memoryUsage < 8192ULL * kMB) return MakeKey("4096to8192");
    if (memoryUsage < 16384ULL * kMB) return MakeKey("8192to16384");
    if (memoryUsage < 32768ULL * kMB) return MakeKey("16384to32768");
    return MakeKey("over32768");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys40backgroundCPUUsageToDiagnosticLoggingKeyEd(double cpuUsage) {
    if (cpuUsage < 1.0) return MakeKey("below1");
    if (cpuUsage < 5.0) return MakeKey("1to5");
    if (cpuUsage < 10.0) return MakeKey("5to10");
    if (cpuUsage < 30.0) return MakeKey("10to30");
    if (cpuUsage < 50.0) return MakeKey("30to50");
    if (cpuUsage < 70.0) return MakeKey("50to70");
    return MakeKey("over70");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys40foregroundCPUUsageToDiagnosticLoggingKeyEd(double cpuUsage) {
    if (cpuUsage < 10.0) return MakeKey("below10");
    if (cpuUsage < 20.0) return MakeKey("10to20");
    if (cpuUsage < 40.0) return MakeKey("20to40");
    if (cpuUsage < 60.0) return MakeKey("40to60");
    if (cpuUsage < 80.0) return MakeKey("60to80");
    return MakeKey("over80");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys42wastedSpeculativeWarmupWithRevalidationKeyEv() {
    return MakeKey("wastedSpeculativeWarmupWithRevalidation");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys45wastedSpeculativeWarmupWithoutRevalidationKeyEv() {
    return MakeKey("wastedSpeculativeWarmupWithoutRevalidation");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys46successfulSpeculativeWarmupWithRevalidationKeyEv() {
    return MakeKey("successfulSpeculativeWarmupWithRevalidation");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys49successfulSpeculativeWarmupWithoutRevalidationKeyEv() {
    return MakeKey("successfulSpeculativeWarmupWithoutRevalidation");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys8otherKeyEv() {
    return MakeKey("other");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore21DiagnosticLoggingKeys9unusedKeyEv() {
    return MakeKey("unused");
}

}
