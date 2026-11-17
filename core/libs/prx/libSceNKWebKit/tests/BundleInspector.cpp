#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

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

    ExpectErr<std::invalid_argument>([&] { WKBundlePageGetInspector(nullptr); });
    ExpectErr<std::invalid_argument>([&] { WKBundleInspectorShow(nullptr); });
    ExpectErr<std::invalid_argument>([&] { WKBundleInspectorClose(script); });
    ExpectErr<std::invalid_argument>([&] { WKBundleInspectorSetPageProfilingEnabled(nullptr, true); });
    ExpectErr<std::invalid_argument>([&] { WKBundleInspectorEvaluateScriptForTest(nullptr, script); });
    ExpectErr<std::invalid_argument>([&] { WKBundleInspectorEvaluateScriptForTest(ref, nullptr); });
    ExpectErr<std::invalid_argument>([&] { WKBundleInspectorEvaluateScriptForTest(ref, ref); });

    WKRelease(script);
    std::printf("All libSceNKWebKit BundleInspector tests passed!\n");
    return 0;
}
