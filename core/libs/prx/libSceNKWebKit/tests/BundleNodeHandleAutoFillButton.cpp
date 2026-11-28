#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "BundleNodeHandle autofill button check failed at line %u\n", location.line());
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
    auto handle = NKWebKit::Make<NKWebKit::BundleNodeHandle>();
    const void* ref = handle.Get();

    Require(WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(ref) == 0);
    Require(WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(ref) == 0);

    WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(ref, 0);
    Require(WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(ref) == 0);
    Require(WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(ref) == 0);

    WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(ref, 1);
    Require(WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(ref) == 1);
    Require(WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(ref) == 0);

    WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(ref, 1);
    Require(WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(ref) == 1);
    Require(WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(ref) == 0);

    WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(ref, 3);
    Require(WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(ref) == 3);
    Require(WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(ref) == 1);

    WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(ref, 0);
    Require(WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(ref) == 0);
    Require(WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(ref) == 3);

    WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(ref, 0);
    Require(WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(ref) == 0);
    Require(WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(ref) == 3);

    WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(ref, 4);
    Require(WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(ref) == 4);
    Require(WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(ref) == 0);

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    const void* frameRef = frame.Get();

    RequireInvalidArgument([] { WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(nullptr); });
    RequireInvalidArgument([&] { WKBundleNodeHandleGetHTMLInputElementAutoFillButtonType(frameRef); });
    RequireInvalidArgument([] { WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(nullptr); });
    RequireInvalidArgument([&] { WKBundleNodeHandleGetHTMLInputElementLastAutoFillButtonType(frameRef); });
    RequireInvalidArgument([] { WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(nullptr, 1); });
    RequireInvalidArgument([&] { WKBundleNodeHandleSetHTMLInputElementAutoFillButtonEnabledWithButtonType(frameRef, 1); });

    return 0;
}
