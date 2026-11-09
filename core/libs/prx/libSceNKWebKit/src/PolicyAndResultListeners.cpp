#include "prx/libSceNKWebKit/src/PolicyAndResultListeners.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKFormSubmissionListenerContinue(const void* listenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FormSubmissionListener>(listenerRef, __func__);
    listener->continued = true;
}

void APS5_VABI WKFramePolicyListenerDownload(const void* listenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(listenerRef, __func__);
    listener->policy = NKWebKit::FramePolicyListener::Policy::Download;
    listener->websitePolicies = nullptr;
}

void APS5_VABI WKFramePolicyListenerIgnore(const void* listenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(listenerRef, __func__);
    listener->policy = NKWebKit::FramePolicyListener::Policy::Ignore;
    listener->websitePolicies = nullptr;
}

void APS5_VABI WKFramePolicyListenerUse(const void* listenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(listenerRef, __func__);
    listener->policy = NKWebKit::FramePolicyListener::Policy::Use;
    listener->websitePolicies = nullptr;
}

void APS5_VABI WKFramePolicyListenerUseInNewProcess(const void* listenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(listenerRef, __func__);
    listener->policy = NKWebKit::FramePolicyListener::Policy::UseInNewProcess;
    listener->websitePolicies = nullptr;
}

void APS5_VABI WKFramePolicyListenerUseInNewProcessWithPolicies(const void* listenerRef, const void* websitePoliciesRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(listenerRef, __func__);
    listener->policy = NKWebKit::FramePolicyListener::Policy::UseInNewProcess;
    listener->websitePolicies = websitePoliciesRef
        ? NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(websitePoliciesRef, __func__)))
        : nullptr;
}

void APS5_VABI WKFramePolicyListenerUseWithPolicies(const void* listenerRef, const void* websitePoliciesRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(listenerRef, __func__);
    listener->policy = NKWebKit::FramePolicyListener::Policy::Use;
    listener->websitePolicies = websitePoliciesRef
        ? NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(websitePoliciesRef, __func__)))
        : nullptr;
}

void APS5_VABI WKMessageListenerSendReply(const void* listenerRef, const void* replyRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::MessageListener>(listenerRef, __func__);
    listener->replied = true;
    listener->reply = replyRef
        ? NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(replyRef, __func__)))
        : nullptr;
}

void APS5_VABI WKPageRequestStorageAccessConfirmResultListenerCall(const void* listenerRef, bool allowed) {
    auto* listener = NKWebKit::Cast<NKWebKit::RequestStorageAccessConfirmResultListener>(listenerRef, __func__);
    listener->called = true;
    listener->allowed = allowed;
}

void APS5_VABI WKPageRunBeforeUnloadConfirmPanelResultListenerCall(const void* listenerRef, bool result) {
    auto* listener = NKWebKit::Cast<NKWebKit::RunBeforeUnloadConfirmPanelResultListener>(listenerRef, __func__);
    listener->called = true;
    listener->result = result;
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

void APS5_VABI WKPageRunJavaScriptPromptResultListenerCall(const void* listenerRef, const void* resultRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::RunJavaScriptPromptResultListener>(listenerRef, __func__);
    listener->called = true;
    listener->result = resultRef
        ? NKWebKit::Retained<NKWebKit::String>::Share(NKWebKit::Cast<NKWebKit::String>(resultRef, __func__))
        : nullptr;
}

}
