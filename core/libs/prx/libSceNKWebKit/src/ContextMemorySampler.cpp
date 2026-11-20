#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKContextStartMemorySampler(const void* contextRef, const void* intervalRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    const auto* interval = NKWebKit::Cast<NKWebKit::Double>(intervalRef, __func__);
    context->memorySamplerRunning = true;
    context->memorySamplerInterval = interval->value;
}

void APS5_VABI WKContextStopMemorySampler(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->memorySamplerRunning = false;
}

}
