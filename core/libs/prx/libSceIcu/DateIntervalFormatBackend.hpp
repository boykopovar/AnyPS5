#pragma once
#include <cstdint>

struct UFormattedValue;

namespace Icu::DateIntervalFormatBackend {

void* Open(const char* locale, const char16_t* skeleton, std::int32_t skeletonLength, const char16_t* tzID,
           std::int32_t tzIDLength, std::int32_t* status);
void Close(void* formatter);
void* OpenResult(std::int32_t* status);
void CloseResult(void* result);
void FormatToResult(const void* formatter, double fromDate, double toDate, void* result, std::int32_t* status);
const UFormattedValue* ResultAsValue(const void* result, std::int32_t* status);

}
