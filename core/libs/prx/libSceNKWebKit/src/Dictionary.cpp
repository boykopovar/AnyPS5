#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
const void* APS5_VABI WKDictionaryCreate(const void** keys, const void** values, std::size_t numberOfValues) {
    std::map<std::u16string, NKWebKit::Retained<NKWebKit::Object>> entries;
    for (std::size_t i = 0; i < numberOfValues; ++i) {
        const auto* key = NKWebKit::Cast<NKWebKit::String>(keys[i], __func__);
        entries.insert_or_assign(key->characters, NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(values[i], __func__))));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Dictionary>(std::move(entries)));
}

void* APS5_VABI WKMutableDictionaryCreate() {
    auto dictionary = NKWebKit::Make<NKWebKit::Dictionary>();
    dictionary->mutableDictionary = true;
    return const_cast<void*>(NKWebKit::ToRef(std::move(dictionary)));
}

const void* APS5_VABI WKDictionaryGetItemForKey(const void* dictionaryRef, const void* keyRef) {
    const auto* dictionary = NKWebKit::Cast<NKWebKit::Dictionary>(dictionaryRef, __func__);
    const auto* key = NKWebKit::Cast<NKWebKit::String>(keyRef, __func__);
    auto it = dictionary->entries.find(key->characters);
    if (it == dictionary->entries.end()) return nullptr;
    return it->second.Get();
}

std::size_t APS5_VABI WKDictionaryGetSize(const void* dictionaryRef) {
    const auto* dictionary = NKWebKit::Cast<NKWebKit::Dictionary>(dictionaryRef, __func__);
    return dictionary->entries.size();
}

const void* APS5_VABI WKDictionaryCopyKeys(const void* dictionaryRef) {
    const auto* dictionary = NKWebKit::Cast<NKWebKit::Dictionary>(dictionaryRef, __func__);
    std::vector<NKWebKit::Retained<NKWebKit::Object>> keys;
    keys.reserve(dictionary->entries.size());
    for (const auto& [k, v] : dictionary->entries) {
        keys.push_back(NKWebKit::Make<NKWebKit::String>(k));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(keys)));
}

bool APS5_VABI WKDictionarySetItem(void* dictionaryRef, const void* keyRef, const void* itemRef) {
    auto* dictionary = NKWebKit::Cast<NKWebKit::Dictionary>(dictionaryRef, __func__);
    const auto* key = NKWebKit::Cast<NKWebKit::String>(keyRef, __func__);
    auto [it, inserted] = dictionary->entries.insert_or_assign(key->characters, NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(itemRef, __func__))));
    return inserted;
}

}
