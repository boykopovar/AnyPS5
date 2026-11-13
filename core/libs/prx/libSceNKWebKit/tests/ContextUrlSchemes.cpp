#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ContextUrlSchemes check failed at line %u\n", location.line());
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

}

int main() {
    auto pool = NKWebKit::Make<NKWebKit::ProcessPool>();
    const void* poolRef = NKWebKit::ToRef(pool);

    auto strEmpty = NKWebKit::Make<NKWebKit::String>(std::u16string(u"custom-empty"));
    const void* emptyRef = NKWebKit::ToRef(strEmpty);
    Require(pool->emptyDocumentURLSchemes.empty());
    WKContextRegisterURLSchemeAsEmptyDocument(poolRef, emptyRef);
    Require(pool->emptyDocumentURLSchemes.size() == 1);
    Require(pool->emptyDocumentURLSchemes[0] == "custom-empty");
    WKContextRegisterURLSchemeAsEmptyDocument(poolRef, emptyRef);
    Require(pool->emptyDocumentURLSchemes.size() == 1);

    auto strSecure = NKWebKit::Make<NKWebKit::String>(std::u16string(u"custom-secure"));
    const void* secureRef = NKWebKit::ToRef(strSecure);
    Require(pool->secureURLSchemes.empty());
    WKContextRegisterURLSchemeAsSecure(poolRef, secureRef);
    Require(pool->secureURLSchemes.size() == 1);
    Require(pool->secureURLSchemes[0] == "custom-secure");
    WKContextRegisterURLSchemeAsSecure(poolRef, secureRef);
    Require(pool->secureURLSchemes.size() == 1);

    auto strCsp = NKWebKit::Make<NKWebKit::String>(std::u16string(u"custom-csp"));
    const void* cspRef = NKWebKit::ToRef(strCsp);
    Require(pool->bypassingContentSecurityPolicyURLSchemes.empty());
    WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(poolRef, cspRef);
    Require(pool->bypassingContentSecurityPolicyURLSchemes.size() == 1);
    Require(pool->bypassingContentSecurityPolicyURLSchemes[0] == "custom-csp");
    WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(poolRef, cspRef);
    Require(pool->bypassingContentSecurityPolicyURLSchemes.size() == 1);

    auto strCache = NKWebKit::Make<NKWebKit::String>(std::u16string(u"custom-cache"));
    const void* cacheRef = NKWebKit::ToRef(strCache);
    Require(pool->cachePartitionedURLSchemes.empty());
    WKContextRegisterURLSchemeAsCachePartitioned(poolRef, cacheRef);
    Require(pool->cachePartitionedURLSchemes.size() == 1);
    Require(pool->cachePartitionedURLSchemes[0] == "custom-cache");
    WKContextRegisterURLSchemeAsCachePartitioned(poolRef, cacheRef);
    Require(pool->cachePartitionedURLSchemes.size() == 1);

    auto strDisplay = NKWebKit::Make<NKWebKit::String>(std::u16string(u"custom-display"));
    const void* displayRef = NKWebKit::ToRef(strDisplay);
    Require(pool->canDisplayOnlyIfCanRequestURLSchemes.empty());
    WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(poolRef, displayRef);
    Require(pool->canDisplayOnlyIfCanRequestURLSchemes.size() == 1);
    Require(pool->canDisplayOnlyIfCanRequestURLSchemes[0] == "custom-display");
    WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(poolRef, displayRef);
    Require(pool->canDisplayOnlyIfCanRequestURLSchemes.size() == 1);

    auto strRelax = NKWebKit::Make<NKWebKit::String>(std::u16string(u"custom-relax"));
    const void* relaxRef = NKWebKit::ToRef(strRelax);
    Require(pool->domainRelaxationForbiddenURLSchemes.empty());
    WKContextSetDomainRelaxationForbiddenForURLScheme(poolRef, relaxRef);
    Require(pool->domainRelaxationForbiddenURLSchemes.size() == 1);
    Require(pool->domainRelaxationForbiddenURLSchemes[0] == "custom-relax");
    WKContextSetDomainRelaxationForbiddenForURLScheme(poolRef, relaxRef);
    Require(pool->domainRelaxationForbiddenURLSchemes.size() == 1);

    auto strRelax2 = NKWebKit::Make<NKWebKit::String>(std::u16string(u"custom-relax-2"));
    const void* relax2Ref = NKWebKit::ToRef(strRelax2);
    WKContextSetDomainRelaxationForbiddenForURLScheme(poolRef, relax2Ref);
    Require(pool->domainRelaxationForbiddenURLSchemes.size() == 2);
    Require(pool->domainRelaxationForbiddenURLSchemes[1] == "custom-relax-2");

    RequireInvalidArgument([] { WKContextRegisterURLSchemeAsEmptyDocument(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKContextRegisterURLSchemeAsEmptyDocument(poolRef, nullptr); });
    RequireInvalidArgument([&] { WKContextRegisterURLSchemeAsEmptyDocument(nullptr, emptyRef); });

    RequireInvalidArgument([] { WKContextRegisterURLSchemeAsSecure(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKContextRegisterURLSchemeAsSecure(poolRef, nullptr); });
    RequireInvalidArgument([&] { WKContextRegisterURLSchemeAsSecure(nullptr, secureRef); });

    RequireInvalidArgument([] { WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(poolRef, nullptr); });
    RequireInvalidArgument([&] { WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(nullptr, cspRef); });

    RequireInvalidArgument([] { WKContextRegisterURLSchemeAsCachePartitioned(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKContextRegisterURLSchemeAsCachePartitioned(poolRef, nullptr); });
    RequireInvalidArgument([&] { WKContextRegisterURLSchemeAsCachePartitioned(nullptr, cacheRef); });

    RequireInvalidArgument([] { WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(poolRef, nullptr); });
    RequireInvalidArgument([&] { WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(nullptr, displayRef); });

    RequireInvalidArgument([] { WKContextSetDomainRelaxationForbiddenForURLScheme(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKContextSetDomainRelaxationForbiddenForURLScheme(poolRef, nullptr); });
    RequireInvalidArgument([&] { WKContextSetDomainRelaxationForbiddenForURLScheme(nullptr, relaxRef); });

    WKRelease(emptyRef);
    WKRelease(secureRef);
    WKRelease(cspRef);
    WKRelease(cacheRef);
    WKRelease(displayRef);
    WKRelease(relaxRef);
    WKRelease(relax2Ref);
    WKRelease(poolRef);

    return 0;
}
