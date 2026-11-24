#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage session state check failed at line %u\n", location.line());
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

bool APS5_VABI TestFilter(const void*, std::uint32_t, const void*, void*) {
    return true;
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = page.Get();
    auto frame = NKWebKit::Make<NKWebKit::Frame>();

    Require(!page->sessionStateRestored);
    Require(!page->sessionStateRestoredWithNavigation);
    Require(page->sessionStateData.empty());

    {
        const void* copied = WKPageCopySessionState(pageRef, nullptr, nullptr);
        Require(copied != nullptr);
        Require(WKGetTypeID(copied) == WKDataGetTypeID());
        const auto* copiedData = static_cast<const NKWebKit::Data*>(NKWebKit::ToObject(copied, "test"));
        Require(copiedData->bytes.empty());
        WKRelease(copied);
    }

    {
        const void* copied = WKPageCopySessionState(pageRef, reinterpret_cast<void*>(1), nullptr);
        Require(copied != nullptr);
        Require(WKGetTypeID(copied) == WKSessionStateGetTypeID());
        const auto* copiedState = static_cast<const NKWebKit::SessionState*>(NKWebKit::ToObject(copied, "test"));
        Require(copiedState->data.empty());
        WKRelease(copied);
    }

    {
        const void* copied = WKPageCopySessionState(pageRef, reinterpret_cast<void*>(1), TestFilter);
        Require(copied != nullptr);
        Require(WKGetTypeID(copied) == WKSessionStateGetTypeID());
        WKRelease(copied);
    }

    {
        std::vector<std::uint8_t> payload = {10, 20, 30, 40};
        auto state = NKWebKit::Make<NKWebKit::SessionState>(payload);
        WKPageRestoreFromSessionState(pageRef, state.Get());
        Require(page->sessionStateRestored);
        Require(page->sessionStateRestoredWithNavigation);
        Require(page->sessionStateData == payload);
    }

    {
        std::vector<std::uint8_t> dataPayload = {50, 60, 70};
        auto dataObj = NKWebKit::Make<NKWebKit::Data>(dataPayload);
        WKPageRestoreFromSessionState(pageRef, dataObj.Get());
        Require(page->sessionStateRestored);
        Require(page->sessionStateRestoredWithNavigation);
        Require(page->sessionStateData == dataPayload);
    }

    std::vector<std::uint8_t> noNavPayload = {80, 90};
    {
        auto state = NKWebKit::Make<NKWebKit::SessionState>(noNavPayload);
        WKPageRestoreFromSessionStateWithoutNavigation(pageRef, state.Get());
        Require(page->sessionStateRestored);
        Require(!page->sessionStateRestoredWithNavigation);
        Require(page->sessionStateData == noNavPayload);
    }

    {
        const void* copied = WKPageCopySessionState(pageRef, nullptr, nullptr);
        Require(copied != nullptr);
        Require(WKGetTypeID(copied) == WKDataGetTypeID());
        const auto* copiedData = static_cast<const NKWebKit::Data*>(NKWebKit::ToObject(copied, "test"));
        Require(copiedData->bytes == noNavPayload);
        WKRelease(copied);
    }

    {
        const void* copied = WKPageCopySessionState(pageRef, reinterpret_cast<void*>(1), nullptr);
        Require(copied != nullptr);
        Require(WKGetTypeID(copied) == WKSessionStateGetTypeID());
        const auto* copiedState = static_cast<const NKWebKit::SessionState*>(NKWebKit::ToObject(copied, "test"));
        Require(copiedState->data == noNavPayload);
        WKRelease(copied);
    }

    page->isClosed = true;
    page->sessionStateRestored = false;
    page->sessionStateRestoredWithNavigation = false;
    page->sessionStateData.clear();

    {
        std::vector<std::uint8_t> closedPayload = {1, 2, 3};
        auto state = NKWebKit::Make<NKWebKit::SessionState>(closedPayload);
        WKPageRestoreFromSessionState(pageRef, state.Get());
        Require(!page->sessionStateRestored);
        Require(!page->sessionStateRestoredWithNavigation);
        Require(page->sessionStateData.empty());

        WKPageRestoreFromSessionStateWithoutNavigation(pageRef, state.Get());
        Require(!page->sessionStateRestored);
        Require(!page->sessionStateRestoredWithNavigation);
        Require(page->sessionStateData.empty());
    }

    page->isClosed = false;

    RequireInvalidArgument([] { WKPageCopySessionState(nullptr, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageCopySessionState(frame.Get(), nullptr, nullptr); });

    RequireInvalidArgument([] { WKPageRestoreFromSessionState(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageRestoreFromSessionState(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKPageRestoreFromSessionState(frame.Get(), frame.Get()); });
    RequireInvalidArgument([&] { WKPageRestoreFromSessionState(pageRef, frame.Get()); });

    RequireInvalidArgument([] { WKPageRestoreFromSessionStateWithoutNavigation(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageRestoreFromSessionStateWithoutNavigation(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKPageRestoreFromSessionStateWithoutNavigation(frame.Get(), frame.Get()); });
    RequireInvalidArgument([&] { WKPageRestoreFromSessionStateWithoutNavigation(pageRef, frame.Get()); });

    return 0;
}
