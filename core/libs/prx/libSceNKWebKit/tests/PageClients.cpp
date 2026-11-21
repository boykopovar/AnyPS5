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
        std::fprintf(stderr, "WKPage client check failed at line %u\n", location.line());
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

struct TestClient {
    int version = 0;
    const void* clientInfo = nullptr;
    void (APS5_VABI *onEvent)(const void* page, const void* clientInfo) = nullptr;
};

const void* g_lastDeliveredPage = nullptr;
const void* g_lastDeliveredContext = nullptr;
int g_callbackCount = 0;

void APS5_VABI CallbackHandler(const void* page, const void* clientInfo) {
    g_lastDeliveredPage = page;
    g_lastDeliveredContext = clientInfo;
    ++g_callbackCount;
}

void Deliver(const std::optional<NKWebKit::PageClientRegistration>& registration, const void* pageRef) {
    Require(registration.has_value());
    const auto* client = static_cast<const TestClient*>(registration->raw);
    Require(client != nullptr);
    Require(client->onEvent != nullptr);
    client->onEvent(pageRef, registration->clientInfo);
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);

    Require(!page->loaderClient.has_value());
    Require(!page->navigationClient.has_value());
    Require(!page->policyClient.has_value());
    Require(!page->uiClient.has_value());

    int context1 = 101;
    int context2 = 202;
    int context3 = 303;
    int context4 = 404;

    TestClient client1{ 1, &context1, &CallbackHandler };
    TestClient client2{ 2, &context2, &CallbackHandler };
    TestClient client3{ 3, &context3, &CallbackHandler };
    TestClient client4{ 4, &context4, &CallbackHandler };

    WKPageSetPageLoaderClient(pageRef, &client1);
    Require(page->loaderClient.has_value());
    Require(page->loaderClient->version == 1);
    Require(page->loaderClient->clientInfo == &context1);
    Require(page->loaderClient->raw == &client1);
    Require(!page->navigationClient.has_value());
    Require(!page->policyClient.has_value());
    Require(!page->uiClient.has_value());

    Deliver(page->loaderClient, pageRef);
    Require(g_callbackCount == 1);
    Require(g_lastDeliveredPage == pageRef);
    Require(g_lastDeliveredContext == &context1);

    WKPageSetPageNavigationClient(pageRef, &client2);
    Require(page->loaderClient.has_value());
    Require(page->navigationClient.has_value());
    Require(page->navigationClient->version == 2);
    Require(page->navigationClient->clientInfo == &context2);
    Require(page->navigationClient->raw == &client2);
    Require(!page->policyClient.has_value());
    Require(!page->uiClient.has_value());

    Deliver(page->navigationClient, pageRef);
    Require(g_callbackCount == 2);
    Require(g_lastDeliveredPage == pageRef);
    Require(g_lastDeliveredContext == &context2);

    WKPageSetPagePolicyClient(pageRef, &client3);
    Require(page->loaderClient.has_value());
    Require(page->navigationClient.has_value());
    Require(page->policyClient.has_value());
    Require(page->policyClient->version == 3);
    Require(page->policyClient->clientInfo == &context3);
    Require(page->policyClient->raw == &client3);
    Require(!page->uiClient.has_value());

    Deliver(page->policyClient, pageRef);
    Require(g_callbackCount == 3);
    Require(g_lastDeliveredPage == pageRef);
    Require(g_lastDeliveredContext == &context3);

    WKPageSetPageUIClient(pageRef, &client4);
    Require(page->loaderClient.has_value());
    Require(page->navigationClient.has_value());
    Require(page->policyClient.has_value());
    Require(page->uiClient.has_value());
    Require(page->uiClient->version == 4);
    Require(page->uiClient->clientInfo == &context4);
    Require(page->uiClient->raw == &client4);

    Deliver(page->uiClient, pageRef);
    Require(g_callbackCount == 4);
    Require(g_lastDeliveredPage == pageRef);
    Require(g_lastDeliveredContext == &context4);

    WKPageSetPageLoaderClient(pageRef, nullptr);
    Require(!page->loaderClient.has_value());
    Require(page->navigationClient.has_value());
    Require(page->policyClient.has_value());
    Require(page->uiClient.has_value());

    WKPageSetPageNavigationClient(pageRef, nullptr);
    Require(!page->loaderClient.has_value());
    Require(!page->navigationClient.has_value());
    Require(page->policyClient.has_value());
    Require(page->uiClient.has_value());

    WKPageSetPagePolicyClient(pageRef, nullptr);
    Require(!page->loaderClient.has_value());
    Require(!page->navigationClient.has_value());
    Require(!page->policyClient.has_value());
    Require(page->uiClient.has_value());

    WKPageSetPageUIClient(pageRef, nullptr);
    Require(!page->loaderClient.has_value());
    Require(!page->navigationClient.has_value());
    Require(!page->policyClient.has_value());
    Require(!page->uiClient.has_value());

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireInvalidArgument([&client1] { WKPageSetPageLoaderClient(nullptr, &client1); });
    RequireInvalidArgument([prefsRef, &client1] { WKPageSetPageLoaderClient(prefsRef, &client1); });

    RequireInvalidArgument([&client2] { WKPageSetPageNavigationClient(nullptr, &client2); });
    RequireInvalidArgument([prefsRef, &client2] { WKPageSetPageNavigationClient(prefsRef, &client2); });

    RequireInvalidArgument([&client3] { WKPageSetPagePolicyClient(nullptr, &client3); });
    RequireInvalidArgument([prefsRef, &client3] { WKPageSetPagePolicyClient(prefsRef, &client3); });

    RequireInvalidArgument([&client4] { WKPageSetPageUIClient(nullptr, &client4); });
    RequireInvalidArgument([prefsRef, &client4] { WKPageSetPageUIClient(prefsRef, &client4); });

    WKRelease(prefsRef);
    WKRelease(pageRef);
    return 0;
}
