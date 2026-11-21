#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKContextClearCurrentModifierStateForTesting(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->currentModifierStateCleared = true;
}

void APS5_VABI WKContextSetUseSeparateServiceWorkerProcess(const void* contextRef, bool useSeparateServiceWorkerProcess) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->useSeparateServiceWorkerProcess = useSeparateServiceWorkerProcess;
}

void APS5_VABI WKContextWarmInitialProcess(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->initialProcessWarmed = true;
}

}
