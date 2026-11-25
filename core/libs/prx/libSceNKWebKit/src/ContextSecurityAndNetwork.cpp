#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

std::int32_t APS5_VABI WKContextGetNetworkProcessIdentifier(const void* contextRef) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return pool->networkProcessIdentifier;
}

void APS5_VABI WKContextSetPrivilegedNetworkBandwidth(const void* contextRef, std::uint32_t bandwidth) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->privilegedNetworkBandwidth = bandwidth;
}

void APS5_VABI WKContextSetWebSecurityFilter(const void* contextRef, const void* filterRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!filterRef) {
        pool->webSecurityFilter = nullptr;
        return;
    }
    pool->webSecurityFilter = NKWebKit::Retained<NKWebKit::Object>::Share(
        const_cast<NKWebKit::Object*>(NKWebKit::ToObject(filterRef, __func__)));
}

const void* APS5_VABI WKContextGetWebSecurityFilter(const void* contextRef) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return pool->webSecurityFilter ? NKWebKit::ToRef(pool->webSecurityFilter) : nullptr;
}

void APS5_VABI WKContextSetDownloadClient(const void* contextRef, const void* clientRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->downloadClientConfigured = (clientRef != nullptr);
}

}
