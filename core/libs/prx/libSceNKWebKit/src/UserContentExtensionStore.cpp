#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cctype>

extern "C" {

const void* APS5_VABI WKUserContentExtensionStoreCreate(const void* pathRef) {
    std::u16string path;
    if (pathRef != nullptr) {
        path = NKWebKit::Cast<NKWebKit::String>(pathRef, __func__)->characters;
    }
    auto store = NKWebKit::Make<NKWebKit::ContentRuleListStore>();
    store->path = std::move(path);
    return NKWebKit::ToRef(store);
}

void APS5_VABI WKUserContentExtensionStoreCompile(const void* storeRef, const void* identifierRef, const void* jsonSourceRef, void* context, void (*callback)(const void*, std::uint32_t, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::ContentRuleListStore>(storeRef, __func__);
    const auto* identifier = NKWebKit::Cast<NKWebKit::String>(identifierRef, __func__);
    const auto* jsonSource = NKWebKit::Cast<NKWebKit::String>(jsonSourceRef, __func__);

    std::size_t start = 0;
    while (start < jsonSource->characters.size() && (jsonSource->characters[start] <= u' ' || jsonSource->characters[start] == 0x7f)) {
        ++start;
    }
    std::size_t end = jsonSource->characters.size();
    while (end > start && (jsonSource->characters[end - 1] <= u' ' || jsonSource->characters[end - 1] == 0x7f)) {
        --end;
    }

    if (start >= end || (jsonSource->characters[start] != u'[' && jsonSource->characters[start] != u'{') ||
        (jsonSource->characters[start] == u'[' && jsonSource->characters[end - 1] != u']') ||
        (jsonSource->characters[start] == u'{' && jsonSource->characters[end - 1] != u'}')) {
        if (callback) {
            callback(nullptr, 3, context);
        }
        return;
    }

    auto filter = NKWebKit::Make<NKWebKit::ContentRuleList>();
    const void* filterRef = NKWebKit::ToRef(filter);
    store->filters[identifier->characters] = NKWebKit::Retained<NKWebKit::Object>::Share(filter.Get());

    if (callback) {
        callback(filterRef, 0, context);
    }
}

void APS5_VABI WKUserContentExtensionStoreLookup(const void* storeRef, const void* identifierRef, void* context, void (*callback)(const void*, std::uint32_t, void*)) {
    auto* store = NKWebKit::Cast<NKWebKit::ContentRuleListStore>(storeRef, __func__);
    const auto* identifier = NKWebKit::Cast<NKWebKit::String>(identifierRef, __func__);

    auto it = store->filters.find(identifier->characters);
    if (it != store->filters.end()) {
        if (callback) {
            callback(NKWebKit::ToRef(it->second), 0, context);
        }
    } else {
        if (callback) {
            callback(nullptr, 1, context);
        }
    }
}

}
