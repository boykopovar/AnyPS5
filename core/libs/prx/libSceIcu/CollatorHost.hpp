#pragma once
#include <cstdint>

namespace Icu::CollatorHost {

void* Open(const char* locale, std::int32_t& status);
void Close(void* collator);
std::int32_t GetAttribute(const void* collator, std::int32_t attribute, std::int32_t& status);
void SetAttribute(void* collator, std::int32_t attribute, std::int32_t value, std::int32_t& status);
std::int32_t Compare(const void* collator, const char16_t* source, std::int32_t sourceLength, const char16_t* target,
                     std::int32_t targetLength);
std::int32_t CompareUtf8(const void* collator, const char* source, std::int32_t sourceLength, const char* target,
                         std::int32_t targetLength, std::int32_t& status);

} // namespace Icu::CollatorHost
