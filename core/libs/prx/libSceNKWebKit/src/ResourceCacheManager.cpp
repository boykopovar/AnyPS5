#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <stdexcept>
#include <string>

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
    const auto* object = NKWebKit::ToObject(originRef, __func__);
    if (object->Type() == NKWebKit::ObjectType::SecurityOrigin) {
        const auto* origin = static_cast<const NKWebKit::SecurityOrigin*>(object);
        manager->ClearForOrigin(NKWebKit::EncodeUtf8(origin->host), cachesToClear);
    } else if (object->Type() == NKWebKit::ObjectType::String) {
        const auto* str = static_cast<const NKWebKit::String*>(object);
        manager->ClearForOrigin(str->Utf8(), cachesToClear);
    } else {
        throw std::invalid_argument(std::string(__func__) + ": invalid origin reference");
    }
}

void APS5_VABI WKResourceCacheManagerGetCacheOrigins(const void* managerRef, void* context, void (*callback)(const void*, const void*, void*)) {
    auto* manager = NKWebKit::Cast<NKWebKit::ResourceCacheManager>(managerRef, __func__);
    if (callback) {
        auto array = NKWebKit::Make<NKWebKit::Array>();
        for (const auto& o : manager->origins) {
            array->elements.push_back(NKWebKit::Make<NKWebKit::SecurityOrigin>(
                NKWebKit::String(std::u16string(u"https")), NKWebKit::String(NKWebKit::DecodeUtf8(o.c_str())), std::nullopt));
        }
        callback(NKWebKit::ToRef(array), nullptr, context);
    }
}

}
