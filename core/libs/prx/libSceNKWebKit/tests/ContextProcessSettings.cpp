#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Fn>
static void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    const void* contextRef = WKContextCreate();
    Require(contextRef != nullptr);
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);

    Require(!pool->configuration->isAutomaticProcessWarmingEnabledByClient.has_value());
    WKContextSetPrewarmsProcessesAutomatically(contextRef, true);
    Require(pool->configuration->isAutomaticProcessWarmingEnabledByClient.value_or(false));
    WKContextSetPrewarmsProcessesAutomatically(contextRef, false);
    Require(pool->configuration->isAutomaticProcessWarmingEnabledByClient.has_value());
    Require(!pool->configuration->isAutomaticProcessWarmingEnabledByClient.value());

    const void* sharedConfig = WKContextConfigurationCreate();
    const void* contextA = WKContextCreateWithConfiguration(sharedConfig);
    const void* contextB = WKContextCreateWithConfiguration(sharedConfig);
    WKContextSetPrewarmsProcessesAutomatically(contextA, true);
    Require(!WKContextConfigurationPrewarmsProcessesAutomatically(sharedConfig));
    Require(!NKWebKit::Cast<NKWebKit::ProcessPool>(contextB, __func__)->configuration->isAutomaticProcessWarmingEnabledByClient.has_value());
    Require(NKWebKit::Cast<NKWebKit::ProcessPool>(contextA, __func__)->configuration->isAutomaticProcessWarmingEnabledByClient.value_or(false));

    WKContextConfigurationSetPrewarmsProcessesAutomatically(sharedConfig, true);
    const void* contextC = WKContextCreateWithConfiguration(sharedConfig);
    Require(NKWebKit::Cast<NKWebKit::ProcessPool>(contextC, __func__)->configuration->isAutomaticProcessWarmingEnabledByClient.value_or(false));
    WKRelease(contextC);
    WKRelease(contextB);
    WKRelease(contextA);
    WKRelease(sharedConfig);

    const void* wrongType = WKStringCreateWithUTF8CString("not-an-array");
    const void* fontA = WKStringCreateWithUTF8CString("Font A");
    const void* fontB = WKStringCreateWithUTF8CString("Font B");
    const void* values[] = {fontA, contextRef, fontB};
    const void* fonts = WKArrayCreate(values, 3);
    Require(pool->fontAllowList.empty());
    WKContextSetFontAllowList(contextRef, fonts);
    Require(pool->fontAllowList.size() == 2);
    Require(pool->fontAllowList[0] == "Font A");
    Require(pool->fontAllowList[1] == "Font B");

    const void* single[] = {fontB};
    const void* replacement = WKArrayCreate(single, 1);
    WKContextSetFontAllowList(contextRef, replacement);
    Require(pool->fontAllowList.size() == 1);
    Require(pool->fontAllowList[0] == "Font B");

    WKContextSetFontAllowList(contextRef, nullptr);
    Require(pool->fontAllowList.empty());

    RequireThrows([&] { WKContextSetPrewarmsProcessesAutomatically(nullptr, true); });
    RequireThrows([&] { WKContextSetPrewarmsProcessesAutomatically(wrongType, true); });
    RequireThrows([&] { WKContextSetFontAllowList(nullptr, fonts); });
    RequireThrows([&] { WKContextSetFontAllowList(contextRef, wrongType); });

    WKRelease(replacement);
    WKRelease(fonts);
    WKRelease(fontB);
    WKRelease(fontA);
    WKRelease(wrongType);
    WKRelease(contextRef);
    return 0;
}
