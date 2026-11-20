#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <algorithm>
#include <cstdint>

namespace NKWebKit {

std::vector<const void*> CopyGuestRefs(const void* const* values, std::size_t count, const char* function) {
    constexpr std::size_t chunkCount = 4096 / sizeof(const void*);
    if (count > (UINTPTR_MAX - reinterpret_cast<std::uintptr_t>(values)) / sizeof(const void*)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const void* chunk[chunkCount];
    for (std::size_t done = 0; done < count; done += chunkCount) {
        if (!GuestCopyIn_nid_no_patch(chunk, values + done, std::min(count - done, chunkCount) * sizeof(const void*)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    std::vector<const void*> refs(count);
    if (count && !GuestCopyIn_nid_no_patch(refs.data(), values, count * sizeof(const void*))) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return refs;
}

}  // namespace NKWebKit

extern "C" {
const void* APS5_VABI WKArrayCreate(const void** values, std::size_t numberOfValues) {
    if (!values && numberOfValues) throw std::invalid_argument(std::string(__func__) + ": null values array");
    const auto refs = NKWebKit::CopyGuestRefs(values, numberOfValues, __func__);
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(numberOfValues);
    for (std::size_t i = 0; i < numberOfValues; ++i) {
        elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(refs[i], __func__))));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

const void* APS5_VABI WKArrayCreateAdoptingValues(const void** values, std::size_t numberOfValues) {
    if (!values && numberOfValues) throw std::invalid_argument(std::string(__func__) + ": null values array");
    const auto refs = NKWebKit::CopyGuestRefs(values, numberOfValues, __func__);
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(numberOfValues);
    for (std::size_t i = 0; i < numberOfValues; ++i) {
        elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Adopt(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(refs[i], __func__))));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

void* APS5_VABI WKMutableArrayCreate() {
    auto array = NKWebKit::Make<NKWebKit::Array>();
    array->mutableArray = true;
    return const_cast<void*>(NKWebKit::ToRef(std::move(array)));
}

const void* APS5_VABI WKArrayGetItemAtIndex(const void* arrayRef, std::size_t index) {
    const auto* array = NKWebKit::Cast<NKWebKit::Array>(arrayRef, __func__);
    if (index >= array->elements.size()) throw std::out_of_range(std::string(__func__) + ": index out of range");
    return array->elements[index].Get();
}

std::size_t APS5_VABI WKArrayGetSize(const void* arrayRef) {
    const auto* array = NKWebKit::Cast<NKWebKit::Array>(arrayRef, __func__);
    return array->elements.size();
}

void APS5_VABI WKArrayAppendItem(void* arrayRef, const void* itemRef) {
    auto* array = NKWebKit::Cast<NKWebKit::Array>(arrayRef, __func__);
    array->elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(itemRef, __func__))));
}

void APS5_VABI WKArrayRemoveItemAtIndex(void* arrayRef, std::size_t index) {
    auto* array = NKWebKit::Cast<NKWebKit::Array>(arrayRef, __func__);
    if (index >= array->elements.size()) throw std::out_of_range(std::string(__func__) + ": index out of range");
    array->elements.erase(array->elements.begin() + index);
}

}
