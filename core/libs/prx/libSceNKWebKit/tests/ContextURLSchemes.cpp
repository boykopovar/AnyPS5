#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

int main() {
    const void* contextRef = WKContextCreate();
    Require(contextRef != nullptr);
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);

    const void* schemeA = WKStringCreateWithUTF8CString("custom-csp");
    const void* schemeB = WKStringCreateWithUTF8CString("custom-cache");
    const void* schemeC = WKStringCreateWithUTF8CString("custom-display");
    const void* schemeD = WKStringCreateWithUTF8CString("custom-empty");
    const void* schemeE = WKStringCreateWithUTF8CString("custom-secure");

    WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(contextRef, schemeA);
    WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(contextRef, schemeA);
    Require(pool->bypassingCSPSchemes.size() == 1);
    Require(pool->bypassingCSPSchemes[0] == "custom-csp");

    WKContextRegisterURLSchemeAsCachePartitioned(contextRef, schemeB);
    Require(pool->cachePartitionedSchemes.size() == 1);
    Require(pool->cachePartitionedSchemes[0] == "custom-cache");

    WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(contextRef, schemeC);
    Require(pool->canDisplayOnlyIfCanRequestSchemes.size() == 1);
    Require(pool->canDisplayOnlyIfCanRequestSchemes[0] == "custom-display");

    WKContextRegisterURLSchemeAsEmptyDocument(contextRef, schemeD);
    Require(pool->emptyDocumentSchemes.size() == 1);
    Require(pool->emptyDocumentSchemes[0] == "custom-empty");

    WKContextRegisterURLSchemeAsSecure(contextRef, schemeE);
    Require(pool->secureSchemes.size() == 1);
    Require(pool->secureSchemes[0] == "custom-secure");

    ExpectErr<std::invalid_argument>([&] { WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(nullptr, schemeA); });
    ExpectErr<std::invalid_argument>([&] { WKContextRegisterURLSchemeAsCachePartitioned(contextRef, nullptr); });
    ExpectErr<std::invalid_argument>([&] { WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(schemeA, schemeC); });
    ExpectErr<std::invalid_argument>([&] { WKContextRegisterURLSchemeAsEmptyDocument(contextRef, contextRef); });
    ExpectErr<std::invalid_argument>([&] { WKContextRegisterURLSchemeAsSecure(nullptr, nullptr); });

    WKRelease(schemeE);
    WKRelease(schemeD);
    WKRelease(schemeC);
    WKRelease(schemeB);
    WKRelease(schemeA);
    WKRelease(contextRef);
    std::printf("All libSceNKWebKit ContextURLSchemes tests passed!\n");
    return 0;
}
