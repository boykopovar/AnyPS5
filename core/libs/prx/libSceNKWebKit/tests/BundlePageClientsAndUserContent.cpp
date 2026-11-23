#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
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

    int dummyClient = 42;

    Require(!page->uiClientConfigured);
    WKBundlePageSetUIClient(pageRef, &dummyClient);
    Require(page->uiClientConfigured);
    WKBundlePageSetUIClient(pageRef, nullptr);
    Require(!page->uiClientConfigured);

    Require(!page->editorClientConfigured);
    WKBundlePageSetEditorClient(pageRef, &dummyClient);
    Require(page->editorClientConfigured);
    WKBundlePageSetEditorClient(pageRef, nullptr);
    Require(!page->editorClientConfigured);

    Require(!page->formClientConfigured);
    WKBundlePageSetFormClient(pageRef, &dummyClient);
    Require(page->formClientConfigured);
    WKBundlePageSetFormClient(pageRef, nullptr);
    Require(!page->formClientConfigured);

    Require(!page->fullScreenClientConfigured);
    WKBundlePageSetFullScreenClient(pageRef, &dummyClient);
    Require(page->fullScreenClientConfigured);
    WKBundlePageSetFullScreenClient(pageRef, nullptr);
    Require(!page->fullScreenClientConfigured);

    Require(!page->pageLoaderClientConfigured);
    WKBundlePageSetPageLoaderClient(pageRef, &dummyClient);
    Require(page->pageLoaderClientConfigured);
    WKBundlePageSetPageLoaderClient(pageRef, nullptr);
    Require(!page->pageLoaderClientConfigured);

    Require(!page->resourceLoadClientConfigured);
    WKBundlePageSetResourceLoadClient(pageRef, &dummyClient);
    Require(page->resourceLoadClientConfigured);
    WKBundlePageSetResourceLoadClient(pageRef, nullptr);
    Require(!page->resourceLoadClientConfigured);

    RequireThrows([&] { WKBundlePageSetUIClient(nullptr, &dummyClient); });
    RequireThrows([&] { WKBundlePageSetEditorClient(nullptr, &dummyClient); });
    RequireThrows([&] { WKBundlePageSetFormClient(nullptr, &dummyClient); });
    RequireThrows([&] { WKBundlePageSetFullScreenClient(nullptr, &dummyClient); });
    RequireThrows([&] { WKBundlePageSetPageLoaderClient(nullptr, &dummyClient); });
    RequireThrows([&] { WKBundlePageSetResourceLoadClient(nullptr, &dummyClient); });

    auto scriptSource = NKWebKit::String::FromUtf8("console.log('test');");
    WKBundlePageAddUserScript(pageRef, scriptSource.Get(), 1, 1);
    Require(page->userScripts.size() == 1);
    auto* script1 = NKWebKit::Cast<NKWebKit::UserScript>(page->userScripts[0].Get(), __func__);
    Require(script1->injectionTime == 1);
    Require(script1->forMainFrameOnly);
    Require(script1->scriptWorld.Get() == WKBundleScriptWorldNormalWorld());

    WKBundlePageAddUserScript(pageRef, nullptr, 0, 0);
    Require(page->userScripts.size() == 1);

    auto emptySource = NKWebKit::String::FromUtf8("");
    WKBundlePageAddUserScript(pageRef, emptySource.Get(), 0, 0);
    Require(page->userScripts.size() == 1);

    const void* customWorldRef = WKBundleScriptWorldCreateWorld();
    auto customWorld = NKWebKit::Retained<NKWebKit::BundleScriptWorld>::Share(
        NKWebKit::Cast<NKWebKit::BundleScriptWorld>(customWorldRef, __func__));
    auto script2Source = NKWebKit::String::FromUtf8("var x = 10;");
    WKBundlePageAddUserScriptInWorld(pageRef, script2Source.Get(), customWorldRef, 0, 0);
    Require(page->userScripts.size() == 2);
    auto* script2 = NKWebKit::Cast<NKWebKit::UserScript>(page->userScripts[1].Get(), __func__);
    Require(script2->injectionTime == 0);
    Require(!script2->forMainFrameOnly);
    Require(script2->scriptWorld.Get() == customWorld.Get());

    auto script3Source = NKWebKit::String::FromUtf8("var y = 20;");
    WKBundlePageAddUserScriptInWorld(pageRef, script3Source.Get(), nullptr, 1, 0);
    Require(page->userScripts.size() == 3);
    auto* script3 = NKWebKit::Cast<NKWebKit::UserScript>(page->userScripts[2].Get(), __func__);
    Require(script3->injectionTime == 1);
    Require(!script3->forMainFrameOnly);
    Require(script3->scriptWorld.Get() == WKBundleScriptWorldNormalWorld());

    WKBundlePageAddUserScriptInWorld(pageRef, nullptr, customWorldRef, 0, 0);
    Require(page->userScripts.size() == 3);

    auto styleSource = NKWebKit::String::FromUtf8("body { background: red; }");
    WKBundlePageAddUserStyleSheet(pageRef, styleSource.Get(), 1);
    Require(page->userStyleSheets.size() == 1);
    auto* sheet1 = NKWebKit::Cast<NKWebKit::UserStyleSheet>(page->userStyleSheets[0].Get(), __func__);
    Require(sheet1->forMainFrameOnly);

    WKBundlePageAddUserStyleSheet(pageRef, nullptr, 0);
    Require(page->userStyleSheets.size() == 1);

    WKBundlePageAddUserStyleSheet(pageRef, emptySource.Get(), 0);
    Require(page->userStyleSheets.size() == 1);

    RequireThrows([&] { WKBundlePageAddUserScript(nullptr, scriptSource.Get(), 0, 0); });
    RequireThrows([&] { WKBundlePageAddUserScriptInWorld(nullptr, scriptSource.Get(), customWorldRef, 0, 0); });
    RequireThrows([&] { WKBundlePageAddUserStyleSheet(nullptr, styleSource.Get(), 0); });
    RequireThrows([&] { WKBundlePageRemoveAllUserContent(nullptr); });

    WKBundlePageRemoveAllUserContent(pageRef);
    Require(page->userScripts.empty());
    Require(page->userStyleSheets.empty());

    WKRelease(customWorldRef);

    std::printf("All libSceNKWebKit BundlePage clients and user content tests passed!\n");
    return 0;
}
