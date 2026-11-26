#pragma once
#include <cstdint>

namespace Icu::LocaleDisplayNamesBackend {

void* OpenForContext(const char* locale, const std::int32_t* contexts, std::int32_t length, std::int32_t* status);
void Close(void* names);
std::int32_t LocaleDisplayName(const void* names, const char* locale, char16_t* result, std::int32_t maxResultSize,
                               std::int32_t* status);
std::int32_t ScriptDisplayName(const void* names, const char* script, char16_t* result, std::int32_t maxResultSize,
                               std::int32_t* status);
std::int32_t RegionDisplayName(const void* names, const char* region, char16_t* result, std::int32_t maxResultSize,
                               std::int32_t* status);
std::int32_t KeyValueDisplayName(const void* names, const char* key, const char* value, char16_t* result,
                                 std::int32_t maxResultSize, std::int32_t* status);

}
