#include "prx/libSceIcu/LocaleDisplayNamesBackend.hpp"
#include "unicode/uldnames.h"
#include <cstdint>
#include <vector>

namespace Icu::LocaleDisplayNamesBackend {

void* OpenForContext(const char* locale, const std::int32_t* contexts, std::int32_t length, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    std::vector<UDisplayContext> hostContexts;
    if (contexts && length > 0) {
        hostContexts.resize(static_cast<std::size_t>(length));
        for (std::int32_t i = 0; i < length; ++i) {
            hostContexts[static_cast<std::size_t>(i)] = static_cast<UDisplayContext>(contexts[i]);
        }
    }
    void* names = uldn_openForContext(locale, hostContexts.empty() ? nullptr : hostContexts.data(), length, &local);
    *status = static_cast<std::int32_t>(local);
    return names;
}

void Close(void* names) {
    uldn_close(static_cast<ULocaleDisplayNames*>(names));
}

std::int32_t LocaleDisplayName(const void* names, const char* locale, char16_t* result, std::int32_t maxResultSize,
                               std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const std::int32_t length = uldn_localeDisplayName(
        static_cast<const ULocaleDisplayNames*>(names), locale, result, maxResultSize, &local);
    *status = static_cast<std::int32_t>(local);
    return length;
}

std::int32_t ScriptDisplayName(const void* names, const char* script, char16_t* result, std::int32_t maxResultSize,
                               std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const std::int32_t length = uldn_scriptDisplayName(
        static_cast<const ULocaleDisplayNames*>(names), script, result, maxResultSize, &local);
    *status = static_cast<std::int32_t>(local);
    return length;
}

std::int32_t RegionDisplayName(const void* names, const char* region, char16_t* result, std::int32_t maxResultSize,
                               std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const std::int32_t length = uldn_regionDisplayName(
        static_cast<const ULocaleDisplayNames*>(names), region, result, maxResultSize, &local);
    *status = static_cast<std::int32_t>(local);
    return length;
}

std::int32_t KeyValueDisplayName(const void* names, const char* key, const char* value, char16_t* result,
                                 std::int32_t maxResultSize, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    const std::int32_t length = uldn_keyValueDisplayName(
        static_cast<const ULocaleDisplayNames*>(names), key, value, result, maxResultSize, &local);
    *status = static_cast<std::int32_t>(local);
    return length;
}

}
