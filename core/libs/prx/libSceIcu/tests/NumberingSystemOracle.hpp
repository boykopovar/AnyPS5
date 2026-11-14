#pragma once
#include <cstdint>

void* NumberingOracleOpen(const char*, bool, std::int32_t*);
void NumberingOracleClose(void*);
const char* NumberingOracleName(const void*);
std::int8_t NumberingOracleAlgorithmic(const void*);
void* NumberingOracleEnumeration(std::int32_t*);
void NumberingOracleEnumClose(void*);
std::int32_t NumberingOracleCount(void*, std::int32_t*);
const char* NumberingOracleNext(void*, std::int32_t*, std::int32_t*);
const char16_t* NumberingOracleUNext(void*, std::int32_t*, std::int32_t*);
void NumberingOracleReset(void*, std::int32_t*);
