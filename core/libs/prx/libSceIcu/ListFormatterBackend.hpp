#pragma once
#include <cstdint>

namespace Icu::ListFormatterBackend {

void* OpenForType(const char* locale, std::int32_t type, std::int32_t width, std::int32_t* status);
void Close(void* formatter);
void* OpenResult(std::int32_t* status);
void CloseResult(void* result);
const void* ResultAsValue(const void* result, std::int32_t* status);
std::int32_t Format(const void* formatter, const char16_t* const* strings, const std::int32_t* stringLengths,
                    std::int32_t stringCount, char16_t* result, std::int32_t resultCapacity, std::int32_t* status);
void FormatStringsToResult(const void* formatter, const char16_t* const* strings, const std::int32_t* stringLengths,
                           std::int32_t stringCount, void* result, std::int32_t* status);

}
