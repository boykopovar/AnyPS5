#pragma once
#include <cstdint>

struct UFieldPositionIterator;

namespace Icu::NumberFormatterBackend {

void* OpenForSkeletonAndLocale(const char16_t* skeleton, std::int32_t skeletonLen, const char* locale,
                               std::int32_t* status);
void Close(void* formatter);
void* OpenResult(std::int32_t* status);
void CloseResult(void* result);
void FormatDecimal(const void* formatter, const char* value, std::int32_t valueLen, void* result,
                   std::int32_t* status);
void FormatDouble(const void* formatter, double value, void* result, std::int32_t* status);
std::int32_t ResultToString(const void* result, char16_t* buffer, std::int32_t bufferCapacity, std::int32_t* status);
void ResultGetAllFieldPositions(const void* result, UFieldPositionIterator* fpositer, std::int32_t* status);

}
