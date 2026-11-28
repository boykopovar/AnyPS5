#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Fn>
static void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

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

    RequireThrows([&] { WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(nullptr, schemeA); });
    RequireThrows([&] { WKContextRegisterURLSchemeAsCachePartitioned(contextRef, nullptr); });
    RequireThrows([&] { WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(schemeA, schemeC); });
    RequireThrows([&] { WKContextRegisterURLSchemeAsEmptyDocument(contextRef, contextRef); });
    RequireThrows([&] { WKContextRegisterURLSchemeAsSecure(nullptr, nullptr); });

    WKRelease(schemeE);
    WKRelease(schemeD);
    WKRelease(schemeC);
    WKRelease(schemeB);
    WKRelease(schemeA);
    WKRelease(contextRef);
    std::printf("All libSceNKWebKit ContextURLSchemes tests passed!\n");
    return 0;
}
