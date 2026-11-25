#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKError check failed at line %u\n", location.line());
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

}  // namespace

int main() {
    auto error = NKWebKit::Make<NKWebKit::Error>("TestDomain", 404, "https://example.com/fail", "Not Found");

    const void* domainRef = WKErrorCopyDomain(error.Get());
    Require(domainRef != nullptr);
    const auto* domain = static_cast<const NKWebKit::String*>(domainRef);
    Require(domain->Utf8() == "TestDomain");

    Require(WKErrorGetErrorCode(error.Get()) == 404);

    const void* urlRef = WKErrorCopyFailingURL(error.Get());
    Require(urlRef != nullptr);
    const auto* url = static_cast<const NKWebKit::URL*>(urlRef);
    Require(url->text == "https://example.com/fail");

    const void* descRef = WKErrorCopyLocalizedDescription(error.Get());
    Require(descRef != nullptr);
    const auto* desc = static_cast<const NKWebKit::String*>(descRef);
    Require(desc->Utf8() == "Not Found");

    const void* defaultDomainRef = WKErrorCopyWKErrorDomain();
    Require(defaultDomainRef != nullptr);
    const auto* defaultDomain = static_cast<const NKWebKit::String*>(defaultDomainRef);
    Require(defaultDomain->Utf8() == "WebKitErrorDomain");

    auto page = NKWebKit::Make<NKWebKit::BundlePage>();

    RequireInvalidArgument([&] { WKErrorCopyDomain(nullptr); });
    RequireInvalidArgument([&] { WKErrorGetErrorCode(nullptr); });
    RequireInvalidArgument([&] { WKErrorCopyFailingURL(nullptr); });
    RequireInvalidArgument([&] { WKErrorCopyLocalizedDescription(nullptr); });

    RequireInvalidArgument([&] { WKErrorCopyDomain(page.Get()); });
    RequireInvalidArgument([&] { WKErrorGetErrorCode(page.Get()); });
    RequireInvalidArgument([&] { WKErrorCopyFailingURL(page.Get()); });
    RequireInvalidArgument([&] { WKErrorCopyLocalizedDescription(page.Get()); });

    return 0;
}
