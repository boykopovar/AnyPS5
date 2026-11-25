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
    auto bundleObj = NKWebKit::Make<NKWebKit::Bundle>();
    const void* bundle = bundleObj.Get();

    const void* groupName = WKStringCreateWithUTF8CString("TestGroup");
    const void* pageGroup = WKPageGroupCreateWithIdentifier(groupName);
    auto* group = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroup, "main");

    const void* worldRef = WKBundleScriptWorldCreateWorld();
    auto* world = NKWebKit::Cast<NKWebKit::BundleScriptWorld>(worldRef, "main");

    const void* scriptSrc1 = WKStringCreateWithUTF8CString("console.log('1');");
    const void* scriptSrc2 = WKStringCreateWithUTF8CString("console.log('2');");
    const void* url1 = WKURLCreateWithUTF8CString("https://example.com/page1");
    const void* url2 = WKURLCreateWithUTF8CString("https://example.com/page2");

    const void* pat1 = WKStringCreateWithUTF8CString("https://*.example.com");
    const void* whiteItems[] = { pat1 };
    const void* whitelist = WKArrayCreate(whiteItems, 1);

    WKBundleAddUserScript(bundle, pageGroup, nullptr, scriptSrc1, url1, whitelist, nullptr, 1, 1);
    Require(group->userContentController->userScripts.size() == 1);
    Require(group->userContentController->userScripts[0]->source == u"console.log('1');");
    Require(group->userContentController->userScripts[0]->baseURL == "https://example.com/page1");
    Require(group->userContentController->userScripts[0]->injectionTime == 1);
    Require(group->userContentController->userScripts[0]->forMainFrameOnly == true);
    Require(group->userContentController->userScripts[0]->allowedURLPatterns.size() == 1);
    Require(group->userContentController->userScripts[0]->scriptWorld.Get() == nullptr);

    WKBundleAddUserScript(bundle, pageGroup, worldRef, scriptSrc2, url2, nullptr, nullptr, 0, 0);
    Require(group->userContentController->userScripts.size() == 2);
    Require(group->userContentController->userScripts[1]->scriptWorld.Get() == world);
    Require(group->userContentController->userScripts[1]->injectionTime == 0);
    Require(group->userContentController->userScripts[1]->forMainFrameOnly == false);

    const void* styleSrc1 = WKStringCreateWithUTF8CString("body { background: red; }");
    const void* styleSrc2 = WKStringCreateWithUTF8CString("div { color: blue; }");

    WKBundleAddUserStyleSheet(bundle, pageGroup, nullptr, styleSrc1, url1, nullptr, nullptr, 1);
    Require(group->userStyleSheets.size() == 1);
    const auto* sheet1 = NKWebKit::Cast<NKWebKit::UserStyleSheet>(group->userStyleSheets[0].Get(), "main");
    Require(sheet1->source == u"body { background: red; }");
    Require(sheet1->baseURL == "https://example.com/page1");
    Require(sheet1->forMainFrameOnly == true);
    Require(sheet1->scriptWorld.Get() == nullptr);

    WKBundleAddUserStyleSheet(bundle, pageGroup, worldRef, styleSrc2, url2, nullptr, nullptr, 0);
    Require(group->userStyleSheets.size() == 2);
    const auto* sheet2 = NKWebKit::Cast<NKWebKit::UserStyleSheet>(group->userStyleSheets[1].Get(), "main");
    Require(sheet2->scriptWorld.Get() == world);
    Require(sheet2->forMainFrameOnly == false);

    WKBundleRemoveUserScript(bundle, pageGroup, worldRef, url2);
    Require(group->userContentController->userScripts.size() == 1);
    Require(group->userContentController->userScripts[0]->source == u"console.log('1');");

    WKBundleRemoveUserScripts(bundle, pageGroup, nullptr);
    Require(group->userContentController->userScripts.empty());

    WKBundleRemoveUserStyleSheet(bundle, pageGroup, worldRef, url2);
    Require(group->userStyleSheets.size() == 1);

    WKBundleRemoveUserStyleSheets(bundle, pageGroup, nullptr);
    Require(group->userStyleSheets.empty());

    WKBundleAddUserScript(bundle, pageGroup, nullptr, scriptSrc1, nullptr, nullptr, nullptr, 0, 0);
    WKBundleAddUserStyleSheet(bundle, pageGroup, nullptr, styleSrc1, nullptr, nullptr, nullptr, 0);
    Require(group->userContentController->userScripts.size() == 1);
    Require(group->userStyleSheets.size() == 1);

    WKBundleRemoveAllUserContent(bundle, pageGroup);
    Require(group->userContentController->userScripts.empty());
    Require(group->userStyleSheets.empty());

    const void* locStr = WKStringCreateWithUTF8CString("userSheet.css");
    WKBundleSetUserStyleSheetLocation(bundle, pageGroup, locStr);
    Require(group->userStyleSheetLocation == u"userSheet.css");

    WKBundleSetUserStyleSheetLocation(bundle, pageGroup, nullptr);
    Require(group->userStyleSheetLocation.empty());

    RequireThrows([&] { WKBundleAddUserScript(nullptr, pageGroup, nullptr, scriptSrc1, nullptr, nullptr, nullptr, 0, 0); });
    RequireThrows([&] { WKBundleAddUserScript(bundle, nullptr, nullptr, scriptSrc1, nullptr, nullptr, nullptr, 0, 0); });
    RequireThrows([&] { WKBundleAddUserStyleSheet(nullptr, pageGroup, nullptr, styleSrc1, nullptr, nullptr, nullptr, 0); });
    RequireThrows([&] { WKBundleAddUserStyleSheet(bundle, nullptr, nullptr, styleSrc1, nullptr, nullptr, nullptr, 0); });
    RequireThrows([&] { WKBundleRemoveUserScript(nullptr, pageGroup, nullptr, nullptr); });
    RequireThrows([&] { WKBundleRemoveUserScript(bundle, nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKBundleRemoveUserScripts(nullptr, pageGroup, nullptr); });
    RequireThrows([&] { WKBundleRemoveUserScripts(bundle, nullptr, nullptr); });
    RequireThrows([&] { WKBundleRemoveUserStyleSheet(nullptr, pageGroup, nullptr, nullptr); });
    RequireThrows([&] { WKBundleRemoveUserStyleSheet(bundle, nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKBundleRemoveUserStyleSheets(nullptr, pageGroup, nullptr); });
    RequireThrows([&] { WKBundleRemoveUserStyleSheets(bundle, nullptr, nullptr); });
    RequireThrows([&] { WKBundleRemoveAllUserContent(nullptr, pageGroup); });
    RequireThrows([&] { WKBundleRemoveAllUserContent(bundle, nullptr); });
    RequireThrows([&] { WKBundleSetUserStyleSheetLocation(nullptr, pageGroup, locStr); });
    RequireThrows([&] { WKBundleSetUserStyleSheetLocation(bundle, nullptr, locStr); });

    WKRelease(groupName);
    WKRelease(pageGroup);
    WKRelease(worldRef);
    WKRelease(scriptSrc1);
    WKRelease(scriptSrc2);
    WKRelease(url1);
    WKRelease(url2);
    WKRelease(pat1);
    WKRelease(whitelist);
    WKRelease(styleSrc1);
    WKRelease(styleSrc2);
    WKRelease(locStr);

    std::printf("All libSceNKWebKit BundleUserContent tests passed!\n");
    return 0;
}
