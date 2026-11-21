#pragma once
#include <cstdint>

void ResourceOracleRegister(const void*, std::int32_t*);
void ResourceOracleCleanup();
std::int32_t ResourceOracleNoFiles();
void* ResourceOracleOpen(const char*, const char*, bool, std::int32_t*);
void ResourceOracleClose(void*);
void* ResourceOracleChild(const void*, const char*, std::int32_t, bool, void*, std::int32_t*);
std::int32_t ResourceOracleSize(const void*);
std::int32_t ResourceOracleType(const void*);
const char* ResourceOracleKey(const void*);
const char16_t* ResourceOracleString(const void*, std::int32_t, const char*, std::int32_t, std::int32_t*, std::int32_t*);
std::int32_t ResourceOracleInt(const void*, std::int32_t*);
const std::uint8_t* ResourceOracleBinary(const void*, std::int32_t*, std::int32_t*);
const std::int32_t* ResourceOracleVector(const void*, std::int32_t*, std::int32_t*);
