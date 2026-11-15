#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

void RegisterScheme(const void* contextRef, const void* urlSchemeRef, std::vector<std::string> NKWebKit::ProcessPool::*member, const char* func) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, func);
    const auto* scheme = NKWebKit::Cast<NKWebKit::String>(urlSchemeRef, func);
    auto& schemes = context->*member;
    auto utf8 = scheme->Utf8();
    if (std::find(schemes.begin(), schemes.end(), utf8) == schemes.end()) {
        schemes.push_back(std::move(utf8));
    }
}

}  // namespace

extern "C" {

void APS5_VABI WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(const void* contextRef, const void* urlSchemeRef) {
    RegisterScheme(contextRef, urlSchemeRef, &NKWebKit::ProcessPool::bypassingCSPSchemes, __func__);
}

void APS5_VABI WKContextRegisterURLSchemeAsCachePartitioned(const void* contextRef, const void* urlSchemeRef) {
    RegisterScheme(contextRef, urlSchemeRef, &NKWebKit::ProcessPool::cachePartitionedSchemes, __func__);
}

void APS5_VABI WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(const void* contextRef, const void* urlSchemeRef) {
    RegisterScheme(contextRef, urlSchemeRef, &NKWebKit::ProcessPool::canDisplayOnlyIfCanRequestSchemes, __func__);
}

void APS5_VABI WKContextRegisterURLSchemeAsEmptyDocument(const void* contextRef, const void* urlSchemeRef) {
    RegisterScheme(contextRef, urlSchemeRef, &NKWebKit::ProcessPool::emptyDocumentSchemes, __func__);
}

void APS5_VABI WKContextRegisterURLSchemeAsSecure(const void* contextRef, const void* urlSchemeRef) {
    RegisterScheme(contextRef, urlSchemeRef, &NKWebKit::ProcessPool::secureSchemes, __func__);
}

}
