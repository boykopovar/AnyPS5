#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

namespace {

struct ClientBase {
    int version;
    const void* clientInfo;
};

void SetClient(std::optional<NKWebKit::PageClientRegistration>& slot, const void* client) {
    if (!client) {
        slot.reset();
        return;
    }
    const auto* base = static_cast<const ClientBase*>(client);
    slot = NKWebKit::PageClientRegistration{ base->version, base->clientInfo, client };
}

}

extern "C" {

void APS5_VABI WKPageSetPageContextMenuClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    SetClient(page->contextMenuClient, client);
}

void APS5_VABI WKPageSetPageDiagnosticLoggingClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    SetClient(page->diagnosticLoggingClient, client);
}

void APS5_VABI WKPageSetPageFindClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    SetClient(page->findClient, client);
}

void APS5_VABI WKPageSetPageFindMatchesClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    SetClient(page->findMatchesClient, client);
}

void APS5_VABI WKPageSetPageFormClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    SetClient(page->formClient, client);
}

void APS5_VABI WKPageSetPageInjectedBundleClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    SetClient(page->injectedBundleClient, client);
}

void APS5_VABI WKPageSetPageLoaderClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    SetClient(page->loaderClient, client);
}

void APS5_VABI WKPageSetPageNavigationClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    SetClient(page->navigationClient, client);
}

void APS5_VABI WKPageSetPagePolicyClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    SetClient(page->policyClient, client);
}

void APS5_VABI WKPageSetPageStateClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    SetClient(page->stateClient, client);
}

void APS5_VABI WKPageSetPageUIClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    SetClient(page->uiClient, client);
}

}
