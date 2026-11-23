#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKBundlePage application cache check failed at line %u\n", location.line());
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
    auto page = NKWebKit::Make<NKWebKit::BundlePage>();
    const void* pageRef = page.Get();

    auto origin1 = NKWebKit::Make<NKWebKit::String>(std::u16string(u"https://example.com"));
    auto origin2 = NKWebKit::Make<NKWebKit::String>(std::u16string(u"https://webkit.org"));

    Require(WKBundlePageGetAppCacheUsageForOrigin(pageRef, origin1.Get()) == 0);

    const void* emptyOriginsRef = WKBundlePageCopyOriginsWithApplicationCache(pageRef);
    auto* emptyOrigins = NKWebKit::Cast<NKWebKit::Array>(emptyOriginsRef, "main");
    Require(emptyOrigins->elements.empty());

    WKBundlePageSetApplicationCacheOriginQuota(pageRef, origin1.Get(), 1048576);
    Require(page->appCacheOriginQuotas["https_example.com_443"] == 1048576);

    const void* quotaOnlyOriginsRef = WKBundlePageCopyOriginsWithApplicationCache(pageRef);
    auto* quotaOnlyOrigins = NKWebKit::Cast<NKWebKit::Array>(quotaOnlyOriginsRef, "main");
    Require(quotaOnlyOrigins->elements.empty());

    page->appCacheOriginUsage["https_example.com_443"] = 65536;
    Require(WKBundlePageGetAppCacheUsageForOrigin(pageRef, origin1.Get()) == 65536);

    auto testOriginUrl = NKWebKit::Make<NKWebKit::String>(std::u16string(u"https://example.test:8443"));
    page->appCacheOriginUsage["https_example.test_8443"] = 32;

    const void* originsWithUsageRef = WKBundlePageCopyOriginsWithApplicationCache(pageRef);
    auto* originsWithUsage = NKWebKit::Cast<NKWebKit::Array>(originsWithUsageRef, "main");
    Require(originsWithUsage->elements.size() == 2);
    auto* item0 = NKWebKit::Cast<NKWebKit::String>(originsWithUsage->elements[0].Get(), "main");
    auto* item1 = NKWebKit::Cast<NKWebKit::String>(originsWithUsage->elements[1].Get(), "main");
    Require(item0->Utf8() == "https_example.com_443");
    Require(item1->Utf8() == "https_example.test_8443");

    auto mixedCaseUrl = NKWebKit::Make<NKWebKit::String>(std::u16string(u"HTTPS://EXAMPLE.test:8443/resource"));
    Require(WKBundlePageGetAppCacheUsageForOrigin(pageRef, mixedCaseUrl.Get()) == 32);

    auto secOriginMixed = NKWebKit::Make<NKWebKit::SecurityOrigin>(
        NKWebKit::String(std::u16string(u"HTTPS")),
        NKWebKit::String(std::u16string(u"EXAMPLE.test")),
        std::optional<std::uint16_t>(8443)
    );
    Require(WKBundlePageGetAppCacheUsageForOrigin(pageRef, secOriginMixed.Get()) == 32);

    WKBundlePageSetApplicationCacheOriginQuota(pageRef, testOriginUrl.Get(), 2048);
    Require(page->appCacheOriginQuotas["https_example.test_8443"] == 2048);

    WKBundlePageClearApplicationCacheForOrigin(pageRef, mixedCaseUrl.Get());
    Require(WKBundlePageGetAppCacheUsageForOrigin(pageRef, testOriginUrl.Get()) == 0);
    Require(page->appCacheOriginQuotas["https_example.test_8443"] == 2048);

    WKBundlePageResetApplicationCacheOriginQuota(pageRef, testOriginUrl.Get());
    Require(page->appCacheOriginQuotas.find("https_example.test_8443") == page->appCacheOriginQuotas.end());

    auto secOrigin = NKWebKit::Make<NKWebKit::SecurityOrigin>(
        NKWebKit::String(std::u16string(u"http")),
        NKWebKit::String(std::u16string(u"localhost")),
        std::optional<std::uint16_t>(8080)
    );
    WKBundlePageSetApplicationCacheOriginQuota(pageRef, secOrigin.Get(), 500);
    Require(page->appCacheOriginQuotas["http_localhost_8080"] == 500);

    WKBundlePageResetApplicationCacheOriginQuota(pageRef, secOrigin.Get());
    Require(page->appCacheOriginQuotas.find("http_localhost_8080") == page->appCacheOriginQuotas.end());

    Require(page->applicationCacheClearCount == 0);
    WKBundlePageClearApplicationCache(pageRef);
    Require(page->applicationCacheClearCount == 1);
    Require(page->appCacheOriginQuotas.empty());
    Require(page->appCacheOriginUsage.empty());

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();

    RequireInvalidArgument([&] { WKBundlePageClearApplicationCache(nullptr); });
    RequireInvalidArgument([&] { WKBundlePageClearApplicationCache(frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageClearApplicationCacheForOrigin(nullptr, origin1.Get()); });
    RequireInvalidArgument([&] { WKBundlePageClearApplicationCacheForOrigin(frame.Get(), origin1.Get()); });
    RequireInvalidArgument([&] { WKBundlePageClearApplicationCacheForOrigin(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageClearApplicationCacheForOrigin(pageRef, frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageGetAppCacheUsageForOrigin(nullptr, origin1.Get()); });
    RequireInvalidArgument([&] { WKBundlePageGetAppCacheUsageForOrigin(frame.Get(), origin1.Get()); });
    RequireInvalidArgument([&] { WKBundlePageGetAppCacheUsageForOrigin(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageGetAppCacheUsageForOrigin(pageRef, frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageSetApplicationCacheOriginQuota(nullptr, origin1.Get(), 1024); });
    RequireInvalidArgument([&] { WKBundlePageSetApplicationCacheOriginQuota(frame.Get(), origin1.Get(), 1024); });
    RequireInvalidArgument([&] { WKBundlePageSetApplicationCacheOriginQuota(pageRef, nullptr, 1024); });
    RequireInvalidArgument([&] { WKBundlePageSetApplicationCacheOriginQuota(pageRef, frame.Get(), 1024); });

    RequireInvalidArgument([&] { WKBundlePageResetApplicationCacheOriginQuota(nullptr, origin1.Get()); });
    RequireInvalidArgument([&] { WKBundlePageResetApplicationCacheOriginQuota(frame.Get(), origin1.Get()); });
    RequireInvalidArgument([&] { WKBundlePageResetApplicationCacheOriginQuota(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageResetApplicationCacheOriginQuota(pageRef, frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageCopyOriginsWithApplicationCache(nullptr); });
    RequireInvalidArgument([&] { WKBundlePageCopyOriginsWithApplicationCache(frame.Get()); });

    return 0;
}
