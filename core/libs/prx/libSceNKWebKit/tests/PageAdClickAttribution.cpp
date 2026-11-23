#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "PageAdClickAttribution check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

struct DumpState {
    bool called = false;
    void* receivedContext = nullptr;
    std::size_t stringLength = 0;
};

void DumpCallback(const void* dumpResult, void* functionContext) {
    auto* state = static_cast<DumpState*>(functionContext);
    state->called = true;
    state->receivedContext = functionContext;
    if (dumpResult) {
        state->stringLength = WKStringGetLength(dumpResult);
    }
}

}  // namespace

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);

    WKPageMarkAdClickAttributionsAsExpiredForTesting(pageRef);
    Require(page->adClickAttributionsExpiredForTesting);
    Require(!page->adClickAttributionCleared);

    WKPageSetAdClickAttributionOverrideTimerForTesting(pageRef, true);
    Require(page->adClickAttributionOverrideTimer);
    WKPageSetAdClickAttributionOverrideTimerForTesting(pageRef, false);
    Require(!page->adClickAttributionOverrideTimer);

    auto testURL = NKWebKit::Make<NKWebKit::URL>("https://example.com/attribution");
    const void* urlRef = NKWebKit::ToRef(testURL);
    WKPageSetAdClickAttributionConversionURLForTesting(pageRef, urlRef);
    Require(page->adClickAttributionConversionURL.Get() != nullptr);

    WKPageSetAdClickAttributionConversionURLForTesting(pageRef, nullptr);
    Require(page->adClickAttributionConversionURL.Get() == nullptr);

    WKPageClearAdClickAttribution(pageRef);
    Require(page->adClickAttributionCleared);
    Require(!page->adClickAttributionsExpiredForTesting);
    Require(page->adClickAttributionConversionURL.Get() == nullptr);
    Require(!page->adClickAttributionOverrideTimer);

    DumpState state{};
    WKPageDumpAdClickAttribution(pageRef, &state, DumpCallback);
    Require(state.called);
    Require(state.receivedContext == &state);

    page->adClickAttributionDumpString = NKWebKit::String::FromUtf8("attribution-dump");
    DumpState state2{};
    WKPageDumpAdClickAttribution(pageRef, &state2, DumpCallback);
    Require(state2.called);
    Require(state2.receivedContext == &state2);
    Require(state2.stringLength == 16);

    auto stringObj = NKWebKit::String::FromUtf8("invalid-url");
    const void* stringRef = NKWebKit::ToRef(stringObj);

    RequireInvalidArgument([&] {
        WKPageClearAdClickAttribution(nullptr);
    });
    RequireInvalidArgument([&] {
        WKPageClearAdClickAttribution(stringRef);
    });
    RequireInvalidArgument([&] {
        WKPageMarkAdClickAttributionsAsExpiredForTesting(nullptr);
    });
    RequireInvalidArgument([&] {
        WKPageMarkAdClickAttributionsAsExpiredForTesting(stringRef);
    });
    RequireInvalidArgument([&] {
        WKPageSetAdClickAttributionOverrideTimerForTesting(nullptr, true);
    });
    RequireInvalidArgument([&] {
        WKPageSetAdClickAttributionOverrideTimerForTesting(stringRef, true);
    });
    RequireInvalidArgument([&] {
        WKPageSetAdClickAttributionConversionURLForTesting(nullptr, urlRef);
    });
    RequireInvalidArgument([&] {
        WKPageSetAdClickAttributionConversionURLForTesting(pageRef, stringRef);
    });
    RequireInvalidArgument([&] {
        WKPageDumpAdClickAttribution(nullptr, &state, DumpCallback);
    });
    RequireInvalidArgument([&] {
        WKPageDumpAdClickAttribution(stringRef, &state, DumpCallback);
    });
    RequireInvalidArgument([&] {
        WKPageDumpAdClickAttribution(pageRef, &state, nullptr);
    });

    return 0;
}
