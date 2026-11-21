#include "prx/libc/include/general/VabiMacros.hpp"
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
    Require(!pool->memorySamplerRunning);
    Require(pool->memorySamplerInterval == 0.0);

    const void* interval1 = WKDoubleCreate(1.5);
    WKContextStartMemorySampler(contextRef, interval1);
    Require(pool->memorySamplerRunning);
    Require(pool->memorySamplerInterval == 1.5);

    const void* interval2 = WKDoubleCreate(0.25);
    WKContextStartMemorySampler(contextRef, interval2);
    Require(pool->memorySamplerRunning);
    Require(pool->memorySamplerInterval == 0.25);

    WKContextStopMemorySampler(contextRef);
    Require(!pool->memorySamplerRunning);

    const void* dummyString = WKStringCreateWithUTF8CString("test");
    const void* dummyBool = WKBooleanCreate(true);

    RequireThrows([&] { WKContextStartMemorySampler(nullptr, interval1); });
    RequireThrows([&] { WKContextStartMemorySampler(contextRef, nullptr); });
    RequireThrows([&] { WKContextStartMemorySampler(dummyString, interval1); });
    RequireThrows([&] { WKContextStartMemorySampler(contextRef, dummyString); });
    RequireThrows([&] { WKContextStartMemorySampler(contextRef, dummyBool); });

    RequireThrows([&] { WKContextStopMemorySampler(nullptr); });
    RequireThrows([&] { WKContextStopMemorySampler(dummyString); });
    RequireThrows([&] { WKContextStopMemorySampler(dummyBool); });

    WKRelease(dummyBool);
    WKRelease(dummyString);
    WKRelease(interval2);
    WKRelease(interval1);
    WKRelease(contextRef);
    return 0;
}
