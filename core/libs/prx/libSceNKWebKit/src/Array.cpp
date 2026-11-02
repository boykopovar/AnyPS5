#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
const void* APS5_VABI WKArrayCreate(const void** values, std::size_t numberOfValues) {
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(numberOfValues);
    for (std::size_t i = 0; i < numberOfValues; ++i) {
        elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(values[i], __func__))));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

const void* APS5_VABI WKArrayCreateAdoptingValues(const void** values, std::size_t numberOfValues) {
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(numberOfValues);
    for (std::size_t i = 0; i < numberOfValues; ++i) {
        elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Adopt(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(values[i], __func__))));
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
