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
    Require(!page->contextMenuClient.has_value());
    Require(!page->diagnosticLoggingClient.has_value());
    Require(!page->findClient.has_value());
    Require(!page->findMatchesClient.has_value());
    Require(!page->formClient.has_value());
    Require(!page->injectedBundleClient.has_value());
    Require(!page->stateClient.has_value());

    int c1 = 1, c2 = 2, c3 = 3, c4 = 4, c5 = 5, c6 = 6, c7 = 7, c8 = 8, c9 = 9, c10 = 10, c11 = 11;

    TestClient client1{ 1, &c1, &CallbackHandler };
    TestClient client2{ 2, &c2, &CallbackHandler };
    TestClient client3{ 3, &c3, &CallbackHandler };
    TestClient client4{ 4, &c4, &CallbackHandler };
    TestClient client5{ 5, &c5, &CallbackHandler };
    TestClient client6{ 6, &c6, &CallbackHandler };
    TestClient client7{ 7, &c7, &CallbackHandler };
    TestClient client8{ 8, &c8, &CallbackHandler };
    TestClient client9{ 9, &c9, &CallbackHandler };
    TestClient client10{ 10, &c10, &CallbackHandler };
    TestClient client11{ 11, &c11, &CallbackHandler };

    WKPageSetPageLoaderClient(pageRef, &client1);
    Deliver(page->loaderClient, pageRef);
    Require(g_callbackCount == 1);
    Require(g_lastDeliveredContext == &c1);

    WKPageSetPageNavigationClient(pageRef, &client2);
    Deliver(page->navigationClient, pageRef);
    Require(g_callbackCount == 2);
    Require(g_lastDeliveredContext == &c2);

    WKPageSetPagePolicyClient(pageRef, &client3);
    Deliver(page->policyClient, pageRef);
    Require(g_callbackCount == 3);
    Require(g_lastDeliveredContext == &c3);

    WKPageSetPageUIClient(pageRef, &client4);
    Deliver(page->uiClient, pageRef);
    Require(g_callbackCount == 4);
    Require(g_lastDeliveredContext == &c4);

    WKPageSetPageContextMenuClient(pageRef, &client5);
    Deliver(page->contextMenuClient, pageRef);
    Require(g_callbackCount == 5);
    Require(g_lastDeliveredContext == &c5);

    WKPageSetPageDiagnosticLoggingClient(pageRef, &client6);
    Deliver(page->diagnosticLoggingClient, pageRef);
    Require(g_callbackCount == 6);
    Require(g_lastDeliveredContext == &c6);

    WKPageSetPageFindClient(pageRef, &client7);
    Deliver(page->findClient, pageRef);
    Require(g_callbackCount == 7);
    Require(g_lastDeliveredContext == &c7);

    WKPageSetPageFindMatchesClient(pageRef, &client8);
    Deliver(page->findMatchesClient, pageRef);
    Require(g_callbackCount == 8);
    Require(g_lastDeliveredContext == &c8);

    WKPageSetPageFormClient(pageRef, &client9);
    Deliver(page->formClient, pageRef);
    Require(g_callbackCount == 9);
    Require(g_lastDeliveredContext == &c9);

    WKPageSetPageInjectedBundleClient(pageRef, &client10);
    Deliver(page->injectedBundleClient, pageRef);
    Require(g_callbackCount == 10);
    Require(g_lastDeliveredContext == &c10);

    WKPageSetPageStateClient(pageRef, &client11);
    Deliver(page->stateClient, pageRef);
    Require(g_callbackCount == 11);
    Require(g_lastDeliveredContext == &c11);

    Require(page->loaderClient.has_value());
    Require(page->navigationClient.has_value());
    Require(page->policyClient.has_value());
    Require(page->uiClient.has_value());
    Require(page->contextMenuClient.has_value());
    Require(page->diagnosticLoggingClient.has_value());
    Require(page->findClient.has_value());
    Require(page->findMatchesClient.has_value());
    Require(page->formClient.has_value());
    Require(page->injectedBundleClient.has_value());
    Require(page->stateClient.has_value());

    WKPageSetPageLoaderClient(pageRef, nullptr);
    WKPageSetPageNavigationClient(pageRef, nullptr);
    WKPageSetPagePolicyClient(pageRef, nullptr);
    WKPageSetPageUIClient(pageRef, nullptr);
    WKPageSetPageContextMenuClient(pageRef, nullptr);
    WKPageSetPageDiagnosticLoggingClient(pageRef, nullptr);
    WKPageSetPageFindClient(pageRef, nullptr);
    WKPageSetPageFindMatchesClient(pageRef, nullptr);
    WKPageSetPageFormClient(pageRef, nullptr);
    WKPageSetPageInjectedBundleClient(pageRef, nullptr);
    WKPageSetPageStateClient(pageRef, nullptr);

    Require(!page->loaderClient.has_value());
    Require(!page->navigationClient.has_value());
    Require(!page->policyClient.has_value());
    Require(!page->uiClient.has_value());
    Require(!page->contextMenuClient.has_value());
    Require(!page->diagnosticLoggingClient.has_value());
    Require(!page->findClient.has_value());
    Require(!page->findMatchesClient.has_value());
    Require(!page->formClient.has_value());
    Require(!page->injectedBundleClient.has_value());
    Require(!page->stateClient.has_value());

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

    RequireInvalidArgument([&client5] { WKPageSetPageContextMenuClient(nullptr, &client5); });
    RequireInvalidArgument([prefsRef, &client5] { WKPageSetPageContextMenuClient(prefsRef, &client5); });

    RequireInvalidArgument([&client6] { WKPageSetPageDiagnosticLoggingClient(nullptr, &client6); });
    RequireInvalidArgument([prefsRef, &client6] { WKPageSetPageDiagnosticLoggingClient(prefsRef, &client6); });

    RequireInvalidArgument([&client7] { WKPageSetPageFindClient(nullptr, &client7); });
    RequireInvalidArgument([prefsRef, &client7] { WKPageSetPageFindClient(prefsRef, &client7); });

    RequireInvalidArgument([&client8] { WKPageSetPageFindMatchesClient(nullptr, &client8); });
    RequireInvalidArgument([prefsRef, &client8] { WKPageSetPageFindMatchesClient(prefsRef, &client8); });

    RequireInvalidArgument([&client9] { WKPageSetPageFormClient(nullptr, &client9); });
    RequireInvalidArgument([prefsRef, &client9] { WKPageSetPageFormClient(prefsRef, &client9); });

    RequireInvalidArgument([&client10] { WKPageSetPageInjectedBundleClient(nullptr, &client10); });
    RequireInvalidArgument([prefsRef, &client10] { WKPageSetPageInjectedBundleClient(prefsRef, &client10); });

    RequireInvalidArgument([&client11] { WKPageSetPageStateClient(nullptr, &client11); });
    RequireInvalidArgument([prefsRef, &client11] { WKPageSetPageStateClient(prefsRef, &client11); });

    WKRelease(prefsRef);
    WKRelease(pageRef);
    return 0;
}
