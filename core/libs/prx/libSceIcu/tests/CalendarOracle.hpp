#pragma once
#include <cstdint>

void* CalendarOracleOpen(const char16_t*, std::int32_t, const char*, std::int32_t, std::int32_t*);
void* CalendarOracleClone(const void*, std::int32_t*);
void CalendarOracleClose(void*);
std::int32_t CalendarOracleGet(const void*, std::int32_t, std::int32_t*);
void CalendarOracleSet(void*, double, bool, std::int32_t*);
