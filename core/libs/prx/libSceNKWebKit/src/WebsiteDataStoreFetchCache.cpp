#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <stdexcept>
#include <string>

namespace {

std::string OriginFromRef(const void* originRef, const char* function) {
    if (!originRef) {
        throw std::invalid_argument(std::string(function) + ": null origin reference");
    }
    const auto* object = NKWebKit::ToObject(originRef, function);
    if (object->Type() == NKWebKit::ObjectType::SecurityOrigin) {
        const auto* origin = static_cast<const NKWebKit::SecurityOrigin*>(object);
        return NKWebKit::EncodeUtf8(origin->host);
    }
    if (object->Type() == NKWebKit::ObjectType::String) {
        const auto* str = static_cast<const NKWebKit::String*>(object);
        return str->Utf8();
    }
    throw std::invalid_argument(std::string(function) + ": invalid origin reference");
}

}

extern "C" {

void APS5_VABI WKWebsiteDataStoreGetFetchCacheOrigins(const void* dataStoreRef, void* context, WKWebsiteDataStoreGetFetchCacheOriginsFunction callback) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    if (!callback) {
        return;
    }
    auto array = NKWebKit::Make<NKWebKit::Array>();
    for (const auto& origin : store->fetchCacheOrigins) {
        array->elements.push_back(NKWebKit::Make<NKWebKit::SecurityOrigin>(
            NKWebKit::String(std::u16string(u"https")), NKWebKit::String(NKWebKit::DecodeUtf8(origin.c_str())), std::nullopt));
    }
    callback(NKWebKit::ToRef(array), context);
}

void APS5_VABI WKWebsiteDataStoreGetFetchCacheSizeForOrigin(const void* dataStoreRef, const void* originRef, void* context, WKWebsiteDataStoreGetFetchCacheSizeForOriginFunction callback) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    const auto origin = OriginFromRef(originRef, __func__);
    if (!callback) {
        return;
    }
    std::uint64_t size = 0;
    const auto it = store->fetchCacheSizes.find(origin);
    if (it != store->fetchCacheSizes.end()) {
        size = it->second;
    }
    callback(size, context);
}

void APS5_VABI WKWebsiteDataStoreRemoveAllFetchCaches(const void* dataStoreRef, void* context, WKWebsiteDataStoreRemoveFetchCacheFunction callback) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    store->removedAllFetchCaches = true;
    store->fetchCacheOrigins.clear();
    store->fetchCacheSizes.clear();
    if (callback) {
        callback(context);
    }
}

void APS5_VABI WKWebsiteDataStoreRemoveFetchCacheForOrigin(const void* dataStoreRef, const void* originRef, void* context, WKWebsiteDataStoreRemoveFetchCacheFunction callback) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    const auto origin = OriginFromRef(originRef, __func__);
    std::erase(store->fetchCacheOrigins, origin);
    store->fetchCacheSizes.erase(origin);
    if (callback) {
        callback(context);
    }
}

}
