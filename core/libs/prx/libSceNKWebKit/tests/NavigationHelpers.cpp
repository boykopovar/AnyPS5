#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/src/NavigationData.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>


int main() {
    ExpectErr<std::invalid_argument>([] {
        WKNavigationActionShouldPerformDownload(nullptr);
    });
    ExpectErr<std::invalid_argument>([] {
        WKNavigationResponseCanShowMIMEType(nullptr);
    });
    ExpectErr<std::invalid_argument>([] {
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
