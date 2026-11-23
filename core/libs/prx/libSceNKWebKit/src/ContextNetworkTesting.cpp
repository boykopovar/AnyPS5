#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKContextResetServiceWorkerFetchTimeoutForTesting(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->serviceWorkerFetchTimeoutForTesting.reset();
}

void APS5_VABI WKContextSetAllowsAnySSLCertificateForServiceWorkerTesting(const void* contextRef, bool allows) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->allowsAnySSLCertificateForServiceWorkerTesting = allows;
}

void APS5_VABI WKContextSetAllowsAnySSLCertificateForWebSocketTesting(const void* contextRef, bool allows) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->allowsAnySSLCertificateForWebSocketTesting = allows;
}

void APS5_VABI WKContextSetServiceWorkerFetchTimeoutForTesting(const void* contextRef, double seconds) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->serviceWorkerFetchTimeoutForTesting = seconds;
}

void APS5_VABI WKContextUseTestingNetworkSession(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->useTestingNetworkSession = true;
}

}
