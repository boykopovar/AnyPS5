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
        std::fprintf(stderr, "WKBundlePage overhang image check failed at line %u\n", location.line());
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

    Require(page->topOverhangImage.Get() == nullptr);
    Require(page->bottomOverhangImage.Get() == nullptr);

    const void* imageRef = WKImageCreate(NKWebKit::SizeValue{32.0, 32.0}, 0);
    Require(imageRef != nullptr);

    WKBundlePageSetTopOverhangImage(pageRef, imageRef);
    Require(page->topOverhangImage.Get() == imageRef);

    WKBundlePageSetBottomOverhangImage(pageRef, imageRef);
    Require(page->bottomOverhangImage.Get() == imageRef);

    WKBundlePageSetTopOverhangImage(pageRef, nullptr);
    Require(page->topOverhangImage.Get() == nullptr);

    WKBundlePageSetBottomOverhangImage(pageRef, nullptr);
    Require(page->bottomOverhangImage.Get() == nullptr);

    RequireInvalidArgument([&] { WKBundlePageSetTopOverhangImage(nullptr, imageRef); });
    RequireInvalidArgument([&] { WKBundlePageSetBottomOverhangImage(nullptr, imageRef); });

    auto stringObj = NKWebKit::Make<NKWebKit::String>();
    RequireInvalidArgument([&] { WKBundlePageSetTopOverhangImage(pageRef, stringObj.Get()); });
    RequireInvalidArgument([&] { WKBundlePageSetBottomOverhangImage(pageRef, stringObj.Get()); });

    return 0;
}
