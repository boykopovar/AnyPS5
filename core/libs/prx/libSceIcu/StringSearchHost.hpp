#pragma once
#include <cstdint>

namespace Icu::StringSearchHost {

void* Open(const char16_t* pattern, std::int32_t patternLength, const char16_t* text, std::int32_t textLength,
           const char* locale, void* breakIterator, std::int32_t& status);
void Close(void* search);
std::int32_t GetMatchedLength(const void* search);
std::int32_t GetAttribute(const void* search, std::int32_t attribute);
bool Reset(void* search, bool force);
void FailNextResetAllocation();
void SetOffset(void* search, std::int32_t position, std::int32_t& status);
void SetAttribute(void* search, std::int32_t attribute, std::int32_t value, std::int32_t& status);
void SetPattern(void* search, const char16_t* pattern, std::int32_t patternLength, std::int32_t& status);
void FailNextPatternAllocation();
void SetText(void* search, const char16_t* text, std::int32_t textLength, std::int32_t& status);
std::int32_t Next(void* search, std::int32_t& status);
void* GetCollator(const void* search);

}

namespace Icu::StringSearchRegistry {

void Cleanup();

}
