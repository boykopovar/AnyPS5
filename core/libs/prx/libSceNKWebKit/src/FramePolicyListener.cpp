#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKFramePolicyListenerUse(const void* policyListenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    listener->Resolve(NKWebKit::FramePolicyListener::Action::Use);
}

void APS5_VABI WKFramePolicyListenerDownload(const void* policyListenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    listener->Resolve(NKWebKit::FramePolicyListener::Action::Download);
}

void APS5_VABI WKFramePolicyListenerIgnore(const void* policyListenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    listener->Resolve(NKWebKit::FramePolicyListener::Action::Ignore);
}

void APS5_VABI WKFramePolicyListenerUseInNewProcess(const void* policyListenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    listener->Resolve(NKWebKit::FramePolicyListener::Action::UseInNewProcess);
}

void APS5_VABI WKFramePolicyListenerUseWithPolicies(const void* policyListenerRef, const void* policiesRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    auto* policies = NKWebKit::CastOrNull<NKWebKit::ObjectOf<NKWebKit::ObjectType::WebsitePolicies>>(policiesRef, __func__);
    listener->Resolve(
        NKWebKit::FramePolicyListener::Action::UseWithPolicies,
        NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::ObjectOf<NKWebKit::ObjectType::WebsitePolicies>*>(policies)));
}

void APS5_VABI WKFramePolicyListenerUseInNewProcessWithPolicies(const void* policyListenerRef, const void* policiesRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FramePolicyListener>(policyListenerRef, __func__);
    auto* policies = NKWebKit::CastOrNull<NKWebKit::ObjectOf<NKWebKit::ObjectType::WebsitePolicies>>(policiesRef, __func__);
    listener->Resolve(
        NKWebKit::FramePolicyListener::Action::UseInNewProcessWithPolicies,
        NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::ObjectOf<NKWebKit::ObjectType::WebsitePolicies>*>(policies)));
}

}
