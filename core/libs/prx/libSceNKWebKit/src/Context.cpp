#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKContextCreate() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProcessPool>());
}

const void* APS5_VABI WKContextCreateWithConfiguration(const void* configurationRef) {
    auto config = configurationRef
        ? NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configurationRef, __func__)->Duplicate()
        : NKWebKit::Make<NKWebKit::ProcessPoolConfiguration>();
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProcessPool>(std::move(config)));
}

const void* APS5_VABI WKContextCreateWithInjectedBundlePath(const void* pathRef) {
    auto config = NKWebKit::Make<NKWebKit::ProcessPoolConfiguration>();
    if (pathRef) {
        config->injectedBundlePath = NKWebKit::OptionalString(pathRef, __func__);
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProcessPool>(std::move(config)));
}

bool APS5_VABI WKContextGetUsesSingleWebProcess(const void* contextRef) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return pool->usesSingleWebProcess;
}

void APS5_VABI WKContextPostMessageToInjectedBundle(const void* contextRef, const void* messageNameRef, const void* messageBodyRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->lastInjectedBundleMessageName = NKWebKit::Retained<NKWebKit::Object>::Share(
        const_cast<NKWebKit::Object*>(NKWebKit::ToObject(messageNameRef, __func__)));
    pool->lastInjectedBundleMessageBody = messageBodyRef
        ? NKWebKit::Retained<NKWebKit::Object>::Share(
              const_cast<NKWebKit::Object*>(NKWebKit::ToObject(messageBodyRef, __func__)))
        : nullptr;
}

void APS5_VABI WKContextSetInitializationUserDataForInjectedBundle(const void* contextRef, const void* userDataRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->injectedBundleInitializationUserData = userDataRef
        ? NKWebKit::Retained<NKWebKit::Object>::Share(
              const_cast<NKWebKit::Object*>(NKWebKit::ToObject(userDataRef, __func__)))
        : nullptr;
}

void APS5_VABI WKContextSetInjectedBundleClient(const void* contextRef, const void* client) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->injectedBundleClientConfigured = (client != nullptr);
}

void APS5_VABI WKContextSetUsesSingleWebProcess(const void* contextRef, bool usesSingleWebProcess) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->usesSingleWebProcess = usesSingleWebProcess;
}

}
