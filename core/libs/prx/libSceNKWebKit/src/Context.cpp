#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <algorithm>

extern "C" {

const void* APS5_VABI WKContextCreate() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProcessPool>());
}

const void* APS5_VABI WKContextCreateWithConfiguration(const void* configurationRef) {
    auto config = configurationRef
        ? NKWebKit::Retained<NKWebKit::ProcessPoolConfiguration>::Share(
              NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configurationRef, __func__))
        : NKWebKit::Make<NKWebKit::ProcessPoolConfiguration>();
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProcessPool>(std::move(config)));
}

const void* APS5_VABI WKContextCreateWithInjectedBundlePath(const void* pathRef) {
    auto config = NKWebKit::Make<NKWebKit::ProcessPoolConfiguration>();
    if (pathRef) {
        config->injectedBundlePath = NKWebKit::OptionalString(pathRef, __func__);
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProcessPool>(std::move(config)));
}

bool APS5_VABI WKContextGetUsesSingleWebProcess(const void* contextRef) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return pool->usesSingleWebProcess;
}

void APS5_VABI WKContextSetUsesSingleWebProcess(const void* contextRef, bool usesSingleWebProcess) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->usesSingleWebProcess = usesSingleWebProcess;
}

void APS5_VABI WKContextRegisterURLSchemeAsEmptyDocument(const void* contextRef, const void* urlSchemeRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    const auto* scheme = NKWebKit::Cast<NKWebKit::String>(urlSchemeRef, __func__);
    auto s = scheme->Utf8();
    if (std::find(pool->emptyDocumentURLSchemes.begin(), pool->emptyDocumentURLSchemes.end(), s) == pool->emptyDocumentURLSchemes.end()) {
        pool->emptyDocumentURLSchemes.push_back(s);
    }
}

void APS5_VABI WKContextRegisterURLSchemeAsSecure(const void* contextRef, const void* urlSchemeRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    const auto* scheme = NKWebKit::Cast<NKWebKit::String>(urlSchemeRef, __func__);
    auto s = scheme->Utf8();
    if (std::find(pool->secureURLSchemes.begin(), pool->secureURLSchemes.end(), s) == pool->secureURLSchemes.end()) {
        pool->secureURLSchemes.push_back(s);
    }
}

void APS5_VABI WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(const void* contextRef, const void* urlSchemeRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    const auto* scheme = NKWebKit::Cast<NKWebKit::String>(urlSchemeRef, __func__);
    auto s = scheme->Utf8();
    if (std::find(pool->bypassingContentSecurityPolicyURLSchemes.begin(), pool->bypassingContentSecurityPolicyURLSchemes.end(), s) == pool->bypassingContentSecurityPolicyURLSchemes.end()) {
        pool->bypassingContentSecurityPolicyURLSchemes.push_back(s);
    }
}

void APS5_VABI WKContextRegisterURLSchemeAsCachePartitioned(const void* contextRef, const void* urlSchemeRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    const auto* scheme = NKWebKit::Cast<NKWebKit::String>(urlSchemeRef, __func__);
    auto s = scheme->Utf8();
    if (std::find(pool->cachePartitionedURLSchemes.begin(), pool->cachePartitionedURLSchemes.end(), s) == pool->cachePartitionedURLSchemes.end()) {
        pool->cachePartitionedURLSchemes.push_back(s);
    }
}

void APS5_VABI WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(const void* contextRef, const void* urlSchemeRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    const auto* scheme = NKWebKit::Cast<NKWebKit::String>(urlSchemeRef, __func__);
    auto s = scheme->Utf8();
    if (std::find(pool->canDisplayOnlyIfCanRequestURLSchemes.begin(), pool->canDisplayOnlyIfCanRequestURLSchemes.end(), s) == pool->canDisplayOnlyIfCanRequestURLSchemes.end()) {
        pool->canDisplayOnlyIfCanRequestURLSchemes.push_back(s);
    }
}

void APS5_VABI WKContextSetDomainRelaxationForbiddenForURLScheme(const void* contextRef, const void* urlSchemeRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    const auto* scheme = NKWebKit::Cast<NKWebKit::String>(urlSchemeRef, __func__);
    auto s = scheme->Utf8();
    if (std::find(pool->domainRelaxationForbiddenURLSchemes.begin(), pool->domainRelaxationForbiddenURLSchemes.end(), s) == pool->domainRelaxationForbiddenURLSchemes.end()) {
        pool->domainRelaxationForbiddenURLSchemes.push_back(s);
    }
}

}
