#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage commands check failed at line %u\n", location.line());
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

struct SimpleCallbackState {
    const void* error = nullptr;
    void* context = nullptr;
    int callCount = 0;
};

void APS5_VABI OnSimpleCallback(const void* error, void* context) {
    auto* state = static_cast<SimpleCallbackState*>(context);
    state->error = error;
    state->context = context;
    state->callCount++;
}

struct ValidateCallbackState {
    const void* command = nullptr;
    bool isEnabled = true;
    std::int32_t state = -1;
    void* context = nullptr;
    int callCount = 0;
};

void APS5_VABI OnValidateCallback(const void* command, bool isEnabled, std::int32_t stateValue, void* context) {
    auto* state = static_cast<ValidateCallbackState*>(context);
    state->command = command;
    state->isEnabled = isEnabled;
    state->state = stateValue;
    state->context = context;
    state->callCount++;
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = page.Get();
    auto frame = NKWebKit::Make<NKWebKit::Frame>();
    auto command = NKWebKit::String::FromUtf8("Copy");
    const void* commandRef = command.Get();

    Require(!page->scrollPositionRestored);
    Require(!page->lastExecutedCommand.has_value());

    WKPageExecuteCommand(pageRef, commandRef);
    Require(page->lastExecutedCommand.has_value());
    Require(*page->lastExecutedCommand == u"Copy");

    WKPageTryRestoreScrollPosition(pageRef);
    Require(page->scrollPositionRestored);

    {
        ValidateCallbackState state{};
        WKPageValidateCommand(pageRef, commandRef, &state, OnValidateCallback);
        Require(state.callCount == 1);
        Require(state.command == commandRef);
        Require(!state.isEnabled);
        Require(state.state == 0);
        Require(state.context == &state);
    }

    {
        SimpleCallbackState state{};
        WKPageForceRepaint(pageRef, &state, OnSimpleCallback);
        Require(state.callCount == 1);
        Require(state.error == nullptr);
        Require(state.context == &state);
    }

    {
        SimpleCallbackState state{};
        WKPageCallAfterNextPresentationUpdate(pageRef, &state, OnSimpleCallback);
        Require(state.callCount == 1);
        Require(state.error == nullptr);
        Require(state.context == &state);
    }

    WKPageValidateCommand(pageRef, commandRef, nullptr, nullptr);
    WKPageForceRepaint(pageRef, nullptr, nullptr);
    WKPageCallAfterNextPresentationUpdate(pageRef, nullptr, nullptr);

    page->isClosed = true;
    page->scrollPositionRestored = false;
    page->lastExecutedCommand.reset();

    {
        ValidateCallbackState vState{};
        SimpleCallbackState sState1{};
        SimpleCallbackState sState2{};

        auto pasteCmd = NKWebKit::String::FromUtf8("Paste");
        WKPageExecuteCommand(pageRef, pasteCmd.Get());
        Require(!page->lastExecutedCommand.has_value());

        WKPageTryRestoreScrollPosition(pageRef);
        Require(!page->scrollPositionRestored);

        WKPageValidateCommand(pageRef, commandRef, &vState, OnValidateCallback);
        Require(vState.callCount == 0);

        WKPageForceRepaint(pageRef, &sState1, OnSimpleCallback);
        Require(sState1.callCount == 0);

        WKPageCallAfterNextPresentationUpdate(pageRef, &sState2, OnSimpleCallback);
        Require(sState2.callCount == 0);
    }

    page->isClosed = false;

    RequireInvalidArgument([] { WKPageExecuteCommand(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageExecuteCommand(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKPageExecuteCommand(nullptr, commandRef); });
    RequireInvalidArgument([&] { WKPageExecuteCommand(frame.Get(), commandRef); });
    RequireInvalidArgument([&] { WKPageExecuteCommand(pageRef, frame.Get()); });

    RequireInvalidArgument([] { WKPageTryRestoreScrollPosition(nullptr); });
    RequireInvalidArgument([&] { WKPageTryRestoreScrollPosition(frame.Get()); });

    RequireInvalidArgument([] { WKPageValidateCommand(nullptr, nullptr, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageValidateCommand(pageRef, nullptr, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageValidateCommand(nullptr, commandRef, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageValidateCommand(frame.Get(), commandRef, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageValidateCommand(pageRef, frame.Get(), nullptr, nullptr); });

    RequireInvalidArgument([] { WKPageForceRepaint(nullptr, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageForceRepaint(frame.Get(), nullptr, nullptr); });

    RequireInvalidArgument([] { WKPageCallAfterNextPresentationUpdate(nullptr, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageCallAfterNextPresentationUpdate(frame.Get(), nullptr, nullptr); });

    return 0;
}
