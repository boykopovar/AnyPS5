#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <string>

extern "C" {

void APS5_VABI WKContextSetPrewarmsProcessesAutomatically(const void* contextRef, bool value) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->configuration->isAutomaticProcessWarmingEnabledByClient = value;
}

void APS5_VABI WKContextSetFontAllowList(const void* contextRef, const void* arrayRef) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    const auto* array = NKWebKit::CastOrNull<NKWebKit::Array>(arrayRef, __func__);
    context->fontAllowList.clear();
    if (array == nullptr) {
        return;
    }
    for (const auto& element : array->elements) {
        if (element && element->Type() == NKWebKit::ObjectType::String) {
            context->fontAllowList.push_back(static_cast<const NKWebKit::String*>(element.Get())->Utf8());
        }
    }
}

}
