#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <utility>

extern "C" {

void APS5_VABI WKPageRunBeforeUnloadConfirmPanelResultListenerCall(const void* listenerRef, bool result) {
    auto* listener = NKWebKit::Cast<NKWebKit::RunBeforeUnloadConfirmPanelResultListener>(listenerRef, __func__);
    listener->called = true;
    listener->result = result;
    if (listener->completionHandler) {
        auto handler = std::exchange(listener->completionHandler, nullptr);
        handler(result);
    }
}

void APS5_VABI WKPageRunJavaScriptAlertResultListenerCall(const void* listenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::RunJavaScriptAlertResultListener>(listenerRef, __func__);
    listener->called = true;
    if (listener->completionHandler) {
        auto handler = std::exchange(listener->completionHandler, nullptr);
        handler();
    }
}

void APS5_VABI WKPageRunJavaScriptConfirmResultListenerCall(const void* listenerRef, bool result) {
    auto* listener = NKWebKit::Cast<NKWebKit::RunJavaScriptConfirmResultListener>(listenerRef, __func__);
    listener->called = true;
    listener->result = result;
    if (listener->completionHandler) {
        auto handler = std::exchange(listener->completionHandler, nullptr);
        handler(result);
    }
}

void APS5_VABI WKPageRunJavaScriptPromptResultListenerCall(const void* listenerRef, const void* stringRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::RunJavaScriptPromptResultListener>(listenerRef, __func__);
    listener->called = true;
    const auto* str = NKWebKit::CastOrNull<NKWebKit::String>(stringRef, __func__);
    listener->result = str ? NKWebKit::Retained<NKWebKit::String>::Share(const_cast<NKWebKit::String*>(str)) : nullptr;
    if (listener->completionHandler) {
        auto handler = std::exchange(listener->completionHandler, nullptr);
        handler(str);
    }
}

}
