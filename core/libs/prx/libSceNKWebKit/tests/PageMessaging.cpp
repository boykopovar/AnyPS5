#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceJsc/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "PageMessaging check failed at line %u\n", location.line());
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

struct RunJsResult {
    bool invoked = false;
    bool hasResult = false;
    bool hasError = false;
    std::string resultJson;
    void* context = nullptr;
};

void APS5_VABI TestRunJsCallback(const void* result, const void* error, void* context) {
    auto* r = static_cast<RunJsResult*>(context);
    r->invoked = true;
    r->hasResult = result != nullptr;
    r->hasError = error != nullptr;
    if (result) {
        const auto* ser = static_cast<const NKWebKit::SerializedScriptValue*>(result);
        r->resultJson = ser->data;
    }
    if (error) {
        Require(WKGetTypeID(error) == WKErrorGetTypeID());
    }
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = page.Get();
    auto bundlePage = NKWebKit::Make<NKWebKit::BundlePage>();
    const void* bundlePageRef = bundlePage.Get();
    auto bundle = NKWebKit::Make<NKWebKit::Bundle>();
    const void* bundleRef = bundle.Get();

    RequireInvalidArgument([] {
        WKPageTryClose(nullptr);
    });
    RequireInvalidArgument([&] {
        WKPageTryClose(bundleRef);
    });

    Require(!page->tryCloseCalled);
    Require(!page->isClosed);
    Require(WKPageTryClose(pageRef));
    Require(page->tryCloseCalled);
    Require(page->isClosed);
    Require(WKPageTryClose(pageRef));

    auto openPage = NKWebKit::Make<NKWebKit::Page>();
    const void* openPageRef = openPage.Get();

    const void* msgName = WKStringCreateWithUTF8CString("test-message");
    const void* msgBody = WKBooleanCreate(true);

    RequireInvalidArgument([&] {
        WKPagePostMessageToInjectedBundle(nullptr, msgName, msgBody);
    });
    RequireInvalidArgument([&] {
        WKPagePostMessageToInjectedBundle(openPageRef, nullptr, msgBody);
    });
    RequireInvalidArgument([&] {
        WKPagePostMessageToInjectedBundle(openPageRef, openPageRef, msgBody);
    });

    WKPagePostMessageToInjectedBundle(openPageRef, msgName, msgBody);
    Require(openPage->lastInjectedBundleMessageName.Get() != nullptr);
    Require(openPage->lastInjectedBundleMessageBody.Get() != nullptr);

    WKPagePostMessageToInjectedBundle(openPageRef, msgName, nullptr);
    Require(openPage->lastInjectedBundleMessageName.Get() != nullptr);
    Require(openPage->lastInjectedBundleMessageBody.Get() == nullptr);

    openPage->isClosed = true;
    openPage->lastInjectedBundleMessageName = nullptr;
    WKPagePostMessageToInjectedBundle(openPageRef, msgName, msgBody);
    Require(openPage->lastInjectedBundleMessageName.Get() == nullptr);

    RequireInvalidArgument([&] {
        WKBundlePagePostMessage(nullptr, msgName, msgBody);
    });
    RequireInvalidArgument([&] {
        WKBundlePagePostMessage(pageRef, msgName, msgBody);
    });
    RequireInvalidArgument([&] {
        WKBundlePagePostMessage(bundlePageRef, nullptr, msgBody);
    });
    RequireInvalidArgument([&] {
        WKBundlePagePostMessage(bundlePageRef, bundlePageRef, msgBody);
    });

    WKBundlePagePostMessage(bundlePageRef, msgName, msgBody);
    Require(bundlePage->lastMessageName.Get() != nullptr);
    Require(bundlePage->lastMessageBody.Get() != nullptr);

    WKBundlePagePostMessage(bundlePageRef, msgName, nullptr);
    Require(bundlePage->lastMessageName.Get() != nullptr);
    Require(bundlePage->lastMessageBody.Get() == nullptr);

    RequireInvalidArgument([&] {
        WKBundlePostMessage(nullptr, msgName, msgBody);
    });
    RequireInvalidArgument([&] {
        WKBundlePostMessage(pageRef, msgName, msgBody);
    });
    RequireInvalidArgument([&] {
        WKBundlePostMessage(bundleRef, nullptr, msgBody);
    });
    RequireInvalidArgument([&] {
        WKBundlePostMessage(bundleRef, bundleRef, msgBody);
    });

    WKBundlePostMessage(bundleRef, msgName, msgBody);
    Require(bundle->lastMessageName.Get() != nullptr);
    Require(bundle->lastMessageBody.Get() != nullptr);

    WKBundlePostMessage(bundleRef, msgName, nullptr);
    Require(bundle->lastMessageName.Get() != nullptr);
    Require(bundle->lastMessageBody.Get() == nullptr);

    auto jsPage = NKWebKit::Make<NKWebKit::Page>();
    const void* jsPageRef = jsPage.Get();
    const void* script = WKStringCreateWithUTF8CString("globalThis.test = 42;");

    RequireInvalidArgument([&] {
        WKPageRunJavaScriptInMainFrame(nullptr, script, nullptr, TestRunJsCallback);
    });
    RequireInvalidArgument([&] {
        WKPageRunJavaScriptInMainFrame(bundleRef, script, nullptr, TestRunJsCallback);
    });
    RequireInvalidArgument([&] {
        WKPageRunJavaScriptInMainFrame(jsPageRef, nullptr, nullptr, TestRunJsCallback);
    });
    RequireInvalidArgument([&] {
        WKPageRunJavaScriptInMainFrame(jsPageRef, jsPageRef, nullptr, TestRunJsCallback);
    });

    WKPageRunJavaScriptInMainFrame(jsPageRef, script, nullptr, nullptr);

    RunJsResult res1{};
    WKPageRunJavaScriptInMainFrame(jsPageRef, script, &res1, TestRunJsCallback);
    Require(res1.invoked);
    Require(res1.hasResult);
    Require(!res1.hasError);
    Require(res1.resultJson == "42");

    const void* scriptRead = WKStringCreateWithUTF8CString("globalThis.test;");
    RunJsResult resRead{};
    WKPageRunJavaScriptInMainFrame(jsPageRef, scriptRead, &resRead, TestRunJsCallback);
    Require(resRead.invoked);
    Require(resRead.hasResult);
    Require(!resRead.hasError);
    Require(resRead.resultJson == "42");
    WKRelease(scriptRead);

    const void* throwingScript = WKStringCreateWithUTF8CString("throw 1;");
    RunJsResult resThrow{};
    WKPageRunJavaScriptInMainFrame(jsPageRef, throwingScript, &resThrow, TestRunJsCallback);
    Require(resThrow.invoked);
    Require(!resThrow.hasResult);
    Require(resThrow.hasError);
    WKRelease(throwingScript);

    jsPage->isClosed = true;
    RunJsResult res2{};
    WKPageRunJavaScriptInMainFrame(jsPageRef, script, &res2, TestRunJsCallback);
    Require(res2.invoked);
    Require(!res2.hasResult);
    Require(!res2.hasError);

    std::u16string scriptChars(u"/*\0*/ 42;", 9);
    auto* rawJsStr = JSStringCreateWithCharacters_nid_postfix(reinterpret_cast<const std::uint16_t*>(scriptChars.data()), scriptChars.size());
    const void* nullScript = WKStringCreateWithJSString(rawJsStr);
    JSStringRelease_nid_postfix(rawJsStr);

    jsPage->isClosed = false;
    RunJsResult resNull{};
    WKPageRunJavaScriptInMainFrame(jsPageRef, nullScript, &resNull, TestRunJsCallback);
    Require(resNull.invoked);
    Require(resNull.hasResult);
    Require(!resNull.hasError);
    Require(resNull.resultJson == "42");
    WKRelease(nullScript);

    const void* funcScript = WKStringCreateWithUTF8CString("(function() {})");
    RunJsResult resFunc{};
    WKPageRunJavaScriptInMainFrame(jsPageRef, funcScript, &resFunc, TestRunJsCallback);
    Require(resFunc.invoked);
    Require(!resFunc.hasResult);
    Require(resFunc.hasError);
    WKRelease(funcScript);

    WKRelease(script);
    WKRelease(msgBody);
    WKRelease(msgName);
    return 0;
}
