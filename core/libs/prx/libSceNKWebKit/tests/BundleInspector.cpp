#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

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
    auto page = NKWebKit::Make<NKWebKit::BundlePage>();
    const void* pageRef = page.Get();
    const void* ref = WKBundlePageGetInspector(pageRef);
    Require(ref != nullptr);
    Require(WKBundlePageGetInspector(pageRef) == ref);
    Require(WKGetTypeID(ref) == WKBundleInspectorGetTypeID());

    const auto* inspector = NKWebKit::Cast<NKWebKit::BundleInspector>(ref, "test");
    Require(!inspector->isVisible);
    Require(!inspector->isProfilingPage);

    WKBundleInspectorShow(ref);
    Require(inspector->isVisible);
    WKBundleInspectorClose(ref);
    Require(!inspector->isVisible);

    WKBundleInspectorSetPageProfilingEnabled(ref, true);
    Require(inspector->isProfilingPage);
    WKBundleInspectorSetPageProfilingEnabled(ref, false);
    Require(!inspector->isProfilingPage);

    const void* script = WKStringCreateWithUTF8CString("1 + 1");
    WKBundleInspectorEvaluateScriptForTest(ref, script);
    Require(inspector->evaluatedScriptsForTest.size() == 1);
    Require(inspector->evaluatedScriptsForTest[0] == "1 + 1");

    RequireThrows([&] { WKBundlePageGetInspector(nullptr); });
    RequireThrows([&] { WKBundleInspectorShow(nullptr); });
    RequireThrows([&] { WKBundleInspectorClose(script); });
    RequireThrows([&] { WKBundleInspectorSetPageProfilingEnabled(nullptr, true); });
    RequireThrows([&] { WKBundleInspectorEvaluateScriptForTest(nullptr, script); });
    RequireThrows([&] { WKBundleInspectorEvaluateScriptForTest(ref, nullptr); });
    RequireThrows([&] { WKBundleInspectorEvaluateScriptForTest(ref, ref); });

    WKRelease(script);
    std::printf("All libSceNKWebKit BundleInspector tests passed!\n");
    return 0;
}
