#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKContextSetMaximumNumberOfProcesses(const void* contextRef, std::uint32_t numberOfProcesses) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->maximumNumberOfProcesses = numberOfProcesses;
}

std::uint32_t APS5_VABI WKContextGetMaximumNumberOfProcesses(const void* contextRef) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return pool->maximumNumberOfProcesses;
}

void APS5_VABI WKContextSetProcessModel(const void* contextRef, std::uint32_t processModel) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->processModel = processModel;
}

void APS5_VABI WKContextSetUsesNetworkProcess(const void* contextRef, bool usesNetworkProcess) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->usesNetworkProcess = usesNetworkProcess;
}

void APS5_VABI WKContextSetDiskCacheSpeculativeValidationEnabled(const void* contextRef, bool enabled) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->diskCacheSpeculativeValidationEnabled = enabled;
}

void APS5_VABI WKContextSetCanHandleHTTPSServerTrustEvaluation(const void* contextRef, bool value) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->canHandleHTTPSServerTrustEvaluation = value;
}

void APS5_VABI WKContextSetCustomWebContentServiceBundleIdentifier(const void* contextRef, const void* bundleIdentifierRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!bundleIdentifierRef) {
        pool->customWebContentServiceBundleIdentifier = nullptr;
        return;
    }
    pool->customWebContentServiceBundleIdentifier = NKWebKit::Retained<NKWebKit::String>::Share(
        NKWebKit::Cast<NKWebKit::String>(bundleIdentifierRef, __func__));
}

void APS5_VABI WKContextTerminateNetworkProcess(const void* contextRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    ++pool->terminatedNetworkProcessesCount;
}

}
