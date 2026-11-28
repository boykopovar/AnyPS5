#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Jsc.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/JscExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

bool APS5_VABI WKPageTryClose(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed) {
        return true;
    }
    page->tryCloseCalled = true;
    page->isClosed = true;
    return true;
}

void APS5_VABI WKPagePostMessageToInjectedBundle(const void* pageRef, const void* messageNameRef, const void* messageBodyRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* name = NKWebKit::Cast<NKWebKit::String>(messageNameRef, __func__);
    const auto* body = messageBodyRef ? NKWebKit::ToObject(messageBodyRef, __func__) : nullptr;
    if (page->isClosed) {
        return;
    }
    page->lastInjectedBundleMessageName = NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::String*>(name));
    page->lastInjectedBundleMessageBody = body ? NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(body)) : nullptr;
}

void APS5_VABI WKBundlePagePostMessage(const void* bundlePageRef, const void* messageNameRef, const void* messageBodyRef) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(bundlePageRef, __func__);
    const auto* name = NKWebKit::Cast<NKWebKit::String>(messageNameRef, __func__);
    const auto* body = messageBodyRef ? NKWebKit::ToObject(messageBodyRef, __func__) : nullptr;
    page->lastMessageName = NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::String*>(name));
    page->lastMessageBody = body ? NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(body)) : nullptr;
}

void APS5_VABI WKBundlePostMessage(const void* bundleRef, const void* messageNameRef, const void* messageBodyRef) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    const auto* name = NKWebKit::Cast<NKWebKit::String>(messageNameRef, __func__);
    const auto* body = messageBodyRef ? NKWebKit::ToObject(messageBodyRef, __func__) : nullptr;
    bundle->lastMessageName = NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::String*>(name));
    bundle->lastMessageBody = body ? NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(body)) : nullptr;
}

void APS5_VABI WKPageRunJavaScriptInMainFrame(const void* pageRef, const void* scriptRef, void* context, WKResultErrorFunction callback) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* script = NKWebKit::Cast<NKWebKit::String>(scriptRef, __func__);
    if (!callback) {
        return;
    }
    if (page->isClosed) {
        callback(nullptr, nullptr, context);
        return;
    }
    if (!page->mainFrameContext) {
        page->mainFrameContext = JSGlobalContextCreate(nullptr);
    }
    auto* ctx = page->mainFrameContext;
    auto* jsScript = JSStringCreateWithCharacters_nid_postfix(reinterpret_cast<const std::uint16_t*>(script->characters.data()), script->characters.size());
    NKWebKit::JSValueRef exception = nullptr;
    auto* result = JSEvaluateScript(ctx, jsScript, nullptr, nullptr, 1, &exception);
    JSStringRelease_nid_postfix(jsScript);
    if (exception) {
        auto error = NKWebKit::Make<NKWebKit::Error>(NKWebKit::String::FromUtf8("JavaScript exception"));
        const auto* err = NKWebKit::ToRef(error);
        callback(nullptr, err, context);
        WKRelease(err);
    } else {
        const auto* serialized = WKSerializedScriptValueCreate(ctx, result, nullptr);
        if (!serialized) {
            auto error = NKWebKit::Make<NKWebKit::Error>(NKWebKit::String::FromUtf8("Serialization failed"));
            const auto* err = NKWebKit::ToRef(error);
            callback(nullptr, err, context);
            WKRelease(err);
        } else {
            callback(serialized, nullptr, context);
            WKRelease(serialized);
        }
    }
}

}
