#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace NKWebKit {

WKContextInvalidMessageFunction g_invalidMessageFunction = nullptr;

WKContextInvalidMessageFunction GetInvalidMessageFunction() {
    return g_invalidMessageFunction;
}

}  // namespace NKWebKit

extern "C" {

void APS5_VABI WKContextStartMemorySampler(const void* contextRef, const void* sampleIntervalRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    const auto* interval = NKWebKit::Cast<NKWebKit::Double>(sampleIntervalRef, __func__);
    if (!std::isfinite(interval->value) || interval->value <= 0.0) {
        throw std::invalid_argument(std::string(__func__) + ": sample interval must be a positive finite number");
    }
    context->memorySamplerRunning = true;
    context->memorySamplerInterval = interval->value;
}

void APS5_VABI WKContextStopMemorySampler(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->memorySamplerRunning = false;
    context->memorySamplerInterval = 0.0;
}

void APS5_VABI WKContextSetInvalidMessageFunction(WKContextInvalidMessageFunction function) {
    NKWebKit::g_invalidMessageFunction = function;
}

void APS5_VABI WKContextSetDomainRelaxationForbiddenForURLScheme(const void* contextRef, const void* urlSchemeRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    const auto* scheme = NKWebKit::Cast<NKWebKit::String>(urlSchemeRef, __func__);
    auto utf8 = scheme->Utf8();
    if (std::find(context->domainRelaxationForbiddenSchemes.begin(), context->domainRelaxationForbiddenSchemes.end(), utf8) == context->domainRelaxationForbiddenSchemes.end()) {
        context->domainRelaxationForbiddenSchemes.push_back(std::move(utf8));
    }
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
    if (!std::isfinite(seconds) || seconds <= 0.0) {
        throw std::invalid_argument(std::string(__func__) + ": timeout must be a positive finite number");
    }
    context->serviceWorkerFetchTimeout = seconds;
}

void APS5_VABI WKContextResetServiceWorkerFetchTimeoutForTesting(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->serviceWorkerFetchTimeout.reset();
}

void APS5_VABI WKContextUseTestingNetworkSession(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->usesTestingNetworkSession = true;
}

void APS5_VABI WKContextSetPrivilegedNetworkBandwidth(const void* contextRef, std::uint32_t bandwidth) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->privilegedNetworkBandwidth = bandwidth;
}

}
