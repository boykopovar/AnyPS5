#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKResourceCacheManagerClearCacheForAllOrigins(const void* managerRef, std::uint32_t cachesToClear) {
    auto* manager = NKWebKit::Cast<NKWebKit::ResourceCacheManager>(managerRef, __func__);
    manager->ClearAll(cachesToClear);
}

void APS5_VABI WKResourceCacheManagerClearCacheForOrigin(const void* managerRef, const void* originRef, std::uint32_t cachesToClear) {
    auto* manager = NKWebKit::Cast<NKWebKit::ResourceCacheManager>(managerRef, __func__);
    if (!originRef) {
        return;
    }
    const auto* str = NKWebKit::CastOrNull<NKWebKit::String>(originRef, __func__);
    if (str) {
        manager->ClearForOrigin(str->Utf8(), cachesToClear);
    } else {
        manager->ClearAll(cachesToClear);
    }
}

void APS5_VABI WKResourceCacheManagerGetCacheOrigins(const void* managerRef, void* context, void (*callback)(const void*, const void*, void*)) {
    auto* manager = NKWebKit::Cast<NKWebKit::ResourceCacheManager>(managerRef, __func__);
    if (callback) {
        auto array = NKWebKit::Make<NKWebKit::Array>();
        for (const auto& o : manager->origins) {
            array->elements.push_back(NKWebKit::String::FromUtf8(o.c_str()));
        }
        callback(NKWebKit::ToRef(array), nullptr, context);
    }
}

}
