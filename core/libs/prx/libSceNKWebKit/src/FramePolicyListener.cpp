#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKFramePolicyListenerDownload(const void* policyListenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    listener->decision = NKWebKit::FramePolicyListener::Decision::Download;
}

void APS5_VABI WKFramePolicyListenerIgnore(const void* policyListenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    listener->decision = NKWebKit::FramePolicyListener::Decision::Ignore;
}

void APS5_VABI WKFramePolicyListenerUse(const void* policyListenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    listener->decision = NKWebKit::FramePolicyListener::Decision::Use;
}

void APS5_VABI WKFramePolicyListenerUseInNewProcess(const void* policyListenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    listener->decision = NKWebKit::FramePolicyListener::Decision::UseInNewProcess;
}

void APS5_VABI WKFramePolicyListenerUseInNewProcessWithPolicies(const void* policyListenerRef, const void* websitePoliciesRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    listener->decision = NKWebKit::FramePolicyListener::Decision::UseInNewProcess;
    auto* policies = NKWebKit::CastOrNull<NKWebKit::WebsitePolicies>(websitePoliciesRef, __func__);
    listener->policies = NKWebKit::Retained<NKWebKit::Object>::Share(policies);
}

void APS5_VABI WKFramePolicyListenerUseWithPolicies(const void* policyListenerRef, const void* websitePoliciesRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    listener->decision = NKWebKit::FramePolicyListener::Decision::Use;
    auto* policies = NKWebKit::CastOrNull<NKWebKit::WebsitePolicies>(websitePoliciesRef, __func__);
    listener->policies = NKWebKit::Retained<NKWebKit::Object>::Share(policies);
}

}
