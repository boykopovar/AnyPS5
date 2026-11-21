#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKContextDisableProcessTermination(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->processTerminationEnabled = false;
}

void APS5_VABI WKContextEnableProcessTermination(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->processTerminationEnabled = true;
}

void APS5_VABI WKContextAddVisitedLink(const void* contextRef, const void* visitedURLRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!visitedURLRef) {
        return;
    }
    const auto* str = NKWebKit::Cast<NKWebKit::String>(visitedURLRef, __func__);
    context->visitedLinks.push_back(str->Utf8());
}

void APS5_VABI WKContextClearVisitedLinks(const void* contextRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->visitedLinks.clear();
}

}
