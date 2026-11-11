#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKPageRunBeforeUnloadConfirmPanelResultListenerCall(const void* listenerRef, bool shouldClose) {
    auto* listener = NKWebKit::Cast<NKWebKit::RunBeforeUnloadConfirmPanelResultListener>(listenerRef, __func__);
    listener->called = true;
    listener->shouldClose = shouldClose;
}

void APS5_VABI WKPageRunJavaScriptAlertResultListenerCall(const void* listenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::RunJavaScriptAlertResultListener>(listenerRef, __func__);
    listener->called = true;
}

void APS5_VABI WKPageRunJavaScriptConfirmResultListenerCall(const void* listenerRef, bool result) {
    auto* listener = NKWebKit::Cast<NKWebKit::RunJavaScriptConfirmResultListener>(listenerRef, __func__);
    listener->called = true;
    listener->result = result;
}

void APS5_VABI WKPageRunJavaScriptPromptResultListenerCall(const void* listenerRef, const void* stringRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::RunJavaScriptPromptResultListener>(listenerRef, __func__);
    listener->called = true;
    listener->value = stringRef ? NKWebKit::Cast<NKWebKit::String>(stringRef, __func__) : nullptr;
}

}
