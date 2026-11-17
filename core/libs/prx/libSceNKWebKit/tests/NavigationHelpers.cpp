#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/src/NavigationData.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "NavigationHelpers check failed at line %u\n", location.line());
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
    RequireInvalidArgument([] {
        WKNavigationActionShouldPerformDownload(nullptr);
    });
    RequireInvalidArgument([] {
        WKNavigationResponseCanShowMIMEType(nullptr);
    });
    RequireInvalidArgument([] {
        WKNavigationDataCopyOriginalRequest(nullptr);
    });

    auto action = NKWebKit::Make<NKWebKit::NavigationAction>(true);
    Require(WKNavigationActionShouldPerformDownload(action.Get()) == true);

    auto actionNo = NKWebKit::Make<NKWebKit::NavigationAction>(false);
    Require(WKNavigationActionShouldPerformDownload(actionNo.Get()) == false);

    auto response = NKWebKit::Make<NKWebKit::NavigationResponse>(true);
    Require(WKNavigationResponseCanShowMIMEType(response.Get()) == true);

    auto responseNo = NKWebKit::Make<NKWebKit::NavigationResponse>(false);
    Require(WKNavigationResponseCanShowMIMEType(responseNo.Get()) == false);

    auto data = NKWebKit::Make<NKWebKit::NavigationData>(nullptr, std::nullopt, std::string("https://example.com/item"));
    auto* req = WKNavigationDataCopyOriginalRequest(data.Get());
    Require(req != nullptr);
    Require(WKGetTypeID(req) == WKURLRequestGetTypeID());
    WKRelease(req);

    auto dataEmpty = NKWebKit::Make<NKWebKit::NavigationData>();
    Require(WKNavigationDataCopyOriginalRequest(dataEmpty.Get()) == nullptr);

    return 0;
}
