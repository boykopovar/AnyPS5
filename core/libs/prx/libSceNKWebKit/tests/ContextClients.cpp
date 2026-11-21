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
        std::fprintf(stderr, "WKContext clients check failed at line %u\n", location.line());
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

void DummyInvalidMessageCallback(const void*) {}

}  // namespace

int main() {
    const void* contextRef = WKContextCreate();
    Require(contextRef != nullptr);
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);

    Require(!pool->clientConfigured);
    Require(!pool->historyClientConfigured);
    Require(NKWebKit::ProcessPool::GetInvalidMessageFunction() == nullptr);

    int dummyClient = 1;
    WKContextSetClient(contextRef, &dummyClient);
    Require(pool->clientConfigured);
    WKContextSetClient(contextRef, nullptr);
    Require(!pool->clientConfigured);

    WKContextSetHistoryClient(contextRef, &dummyClient);
    Require(pool->historyClientConfigured);
    WKContextSetHistoryClient(contextRef, nullptr);
    Require(!pool->historyClientConfigured);

    WKContextSetInvalidMessageFunction(&DummyInvalidMessageCallback);
    Require(NKWebKit::ProcessPool::GetInvalidMessageFunction() == &DummyInvalidMessageCallback);
    WKContextSetInvalidMessageFunction(nullptr);
    Require(NKWebKit::ProcessPool::GetInvalidMessageFunction() == nullptr);

    const void* wrongType = WKStringCreateWithUTF8CString("not_a_context");

    RequireInvalidArgument([&] { WKContextSetClient(nullptr, &dummyClient); });
    RequireInvalidArgument([&] { WKContextSetClient(wrongType, &dummyClient); });
    RequireInvalidArgument([&] { WKContextSetHistoryClient(nullptr, &dummyClient); });
    RequireInvalidArgument([&] { WKContextSetHistoryClient(wrongType, &dummyClient); });

    WKRelease(wrongType);
    WKRelease(contextRef);

    return 0;
}
