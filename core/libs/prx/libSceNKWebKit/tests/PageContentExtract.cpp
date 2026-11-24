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
        std::fprintf(stderr, "WKPage content extract check failed at line %u\n", location.line());
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

struct CallbackState {
    const void* result = nullptr;
    const void* error = nullptr;
    void* context = nullptr;
    int callCount = 0;
};

void APS5_VABI OnResult(const void* result, const void* error, void* context) {
    auto* state = static_cast<CallbackState*>(context);
    state->result = result;
    state->error = error;
    state->context = context;
    state->callCount++;
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = page.Get();
    auto frame = NKWebKit::Make<NKWebKit::Frame>();

    {
        CallbackState state{};
        WKPageGetContentsAsString(pageRef, &state, OnResult);
        Require(state.callCount == 1);
        Require(state.result != nullptr);
        Require(state.error == nullptr);
        Require(state.context == &state);
        Require(WKGetTypeID(state.result) == WKStringGetTypeID());
    }

    {
        CallbackState state{};
        WKPageRenderTreeExternalRepresentation(pageRef, &state, OnResult);
        Require(state.callCount == 1);
        Require(state.result != nullptr);
        Require(state.error == nullptr);
        Require(state.context == &state);
        Require(WKGetTypeID(state.result) == WKStringGetTypeID());
    }

    {
        CallbackState state{};
        WKPageGetBytecodeProfile(pageRef, &state, OnResult);
        Require(state.callCount == 1);
        Require(state.result != nullptr);
        Require(state.error == nullptr);
        Require(state.context == &state);
        Require(WKGetTypeID(state.result) == WKStringGetTypeID());
    }

    {
        CallbackState state{};
        WKPageGetSamplingProfilerOutput(pageRef, &state, OnResult);
        Require(state.callCount == 1);
        Require(state.result != nullptr);
        Require(state.error == nullptr);
        Require(state.context == &state);
        Require(WKGetTypeID(state.result) == WKStringGetTypeID());
    }

    {
        CallbackState state{};
        WKPageGetContentsAsMHTMLData(pageRef, &state, OnResult);
        Require(state.callCount == 1);
        Require(state.result != nullptr);
        Require(state.error == nullptr);
        Require(state.context == &state);
        Require(WKGetTypeID(state.result) == WKDataGetTypeID());
    }

    {
        CallbackState state{};
        WKPageGetSelectionAsWebArchiveData(pageRef, &state, OnResult);
        Require(state.callCount == 1);
        Require(state.result != nullptr);
        Require(state.error == nullptr);
        Require(state.context == &state);
        Require(WKGetTypeID(state.result) == WKDataGetTypeID());
    }

    WKPageGetContentsAsString(pageRef, nullptr, nullptr);
    WKPageRenderTreeExternalRepresentation(pageRef, nullptr, nullptr);
    WKPageGetBytecodeProfile(pageRef, nullptr, nullptr);
    WKPageGetSamplingProfilerOutput(pageRef, nullptr, nullptr);
    WKPageGetContentsAsMHTMLData(pageRef, nullptr, nullptr);
    WKPageGetSelectionAsWebArchiveData(pageRef, nullptr, nullptr);

    page->isClosed = true;

    {
        CallbackState state{};
        WKPageGetContentsAsString(pageRef, &state, OnResult);
        WKPageRenderTreeExternalRepresentation(pageRef, &state, OnResult);
        WKPageGetBytecodeProfile(pageRef, &state, OnResult);
        WKPageGetSamplingProfilerOutput(pageRef, &state, OnResult);
        WKPageGetContentsAsMHTMLData(pageRef, &state, OnResult);
        WKPageGetSelectionAsWebArchiveData(pageRef, &state, OnResult);
        Require(state.callCount == 0);
    }

    page->isClosed = false;

    RequireInvalidArgument([] { WKPageGetContentsAsString(nullptr, nullptr, nullptr); });
    RequireInvalidArgument([] { WKPageRenderTreeExternalRepresentation(nullptr, nullptr, nullptr); });
    RequireInvalidArgument([] { WKPageGetBytecodeProfile(nullptr, nullptr, nullptr); });
    RequireInvalidArgument([] { WKPageGetSamplingProfilerOutput(nullptr, nullptr, nullptr); });
    RequireInvalidArgument([] { WKPageGetContentsAsMHTMLData(nullptr, nullptr, nullptr); });
    RequireInvalidArgument([] { WKPageGetSelectionAsWebArchiveData(nullptr, nullptr, nullptr); });

    RequireInvalidArgument([&] { WKPageGetContentsAsString(frame.Get(), nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageRenderTreeExternalRepresentation(frame.Get(), nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageGetBytecodeProfile(frame.Get(), nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageGetSamplingProfilerOutput(frame.Get(), nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageGetContentsAsMHTMLData(frame.Get(), nullptr, nullptr); });
    RequireInvalidArgument([&] { WKPageGetSelectionAsWebArchiveData(frame.Get(), nullptr, nullptr); });

    return 0;
}
