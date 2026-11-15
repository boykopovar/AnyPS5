#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <algorithm>
#include <cstring>

extern "C" {
const void* APS5_VABI WKStringCreateWithUTF8CString(const char* string) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(NKWebKit::DecodeUtf8(string).value_or(std::u16string())));
}

bool APS5_VABI WKStringIsEmpty(const void* stringRef) {
    const auto* string = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    return string->characters.empty();
}

std::size_t APS5_VABI WKStringGetLength(const void* stringRef) {
    const auto* string = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    return string->characters.size();
}

std::size_t APS5_VABI WKStringGetCharacters(const void* stringRef, unsigned short* buffer, std::size_t bufferLength) {
    const auto* string = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    if (!buffer || bufferLength == 0) return 0;
    const std::size_t count = std::min(string->characters.size(), bufferLength);
    std::memcpy(buffer, string->characters.data(), count * sizeof(char16_t));
    return count;
}

std::size_t APS5_VABI WKStringGetMaximumUTF8CStringSize(const void* stringRef) {
    const auto* string = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    return string->characters.size() * 3 + 1;
}

std::size_t APS5_VABI WKStringGetUTF8CString(const void* stringRef, char* buffer, std::size_t bufferSize) {
    const auto* string = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    return NKWebKit::ConvertUtf16ToUtf8(string->characters, buffer, bufferSize, true);
}

std::size_t APS5_VABI WKStringGetUTF8CStringNonStrict(const void* stringRef, char* buffer, std::size_t bufferSize) {
    const auto* string = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    return NKWebKit::ConvertUtf16ToUtf8(string->characters, buffer, bufferSize, false);
}

bool APS5_VABI WKStringIsEqual(const void* aRef, const void* bRef) {
    const auto* a = NKWebKit::Cast<NKWebKit::String>(aRef, __func__);
    const auto* b = NKWebKit::Cast<NKWebKit::String>(bRef, __func__);
    return a->characters == b->characters;
}

bool APS5_VABI WKStringIsEqualToUTF8CString(const void* aRef, const char* b) {
    const auto* a = NKWebKit::Cast<NKWebKit::String>(aRef, __func__);
    if (!b) return a->characters.empty();
    const auto decoded = NKWebKit::DecodeUtf8(b);
    if (!decoded.has_value()) return a->characters.empty();
    return a->characters == *decoded;
}

bool APS5_VABI WKStringIsEqualToUTF8CStringIgnoringCase(const void* aRef, const char* b) {
    const auto* a = NKWebKit::Cast<NKWebKit::String>(aRef, __func__);
    if (!b) return a->characters.empty();
    const auto decoded = NKWebKit::DecodeUtf8(b);
    if (!decoded.has_value()) return a->characters.empty();
    if (a->characters.size() != decoded->size()) return false;
    for (std::size_t i = 0; i < a->characters.size(); ++i) {
        char16_t ca = a->characters[i];
        char16_t cb = (*decoded)[i];
        if (ca >= 'A' && ca <= 'Z') ca += ('a' - 'A');
        if (cb >= 'A' && cb <= 'Z') cb += ('a' - 'A');
        if (ca != cb) return false;
    }
    return true;
}

const void* APS5_VABI WKStringCreateWithJSString(GuestJSString* jsStringRef) {
    if (!jsStringRef) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    const auto* chars = JSStringGetCharactersPtr_nid_postfix(jsStringRef);
    if (!chars) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string()));
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(reinterpret_cast<const char16_t*>(chars), JSStringGetLength_nid_postfix(jsStringRef))));
}

GuestJSString* APS5_VABI WKStringCopyJSString(const void* stringRef) {
    const auto* string = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    if (string->null) return nullptr;
    return JSStringCreateWithCharacters_nid_postfix(reinterpret_cast<const std::uint16_t*>(string->characters.data()), string->characters.size());
}

}
