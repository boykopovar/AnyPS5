#pragma once
#include <cstdint>

void* PatternOracleOpen(const char* locale, std::int32_t* status);
void PatternOracleClose(void* handle);
std::int32_t PatternOracleConvert(void* handle, std::int32_t operation, const char16_t* input, std::int32_t length,
                                 std::int32_t options, std::int32_t field, std::int32_t width,
                                 char16_t* output, std::int32_t capacity, std::int32_t* status);
