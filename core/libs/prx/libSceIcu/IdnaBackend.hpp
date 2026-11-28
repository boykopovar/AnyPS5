#pragma once
#include <cstdint>

namespace Icu::IdnaBackend {

struct Info {
    std::int16_t size;
    std::int8_t isTransitionalDifferent;
    std::int8_t reservedB3;
    std::uint32_t errors;
    std::int32_t reservedI2;
    std::int32_t reservedI3;
};

void* OpenUTS46(std::uint32_t options, std::int32_t* status);
void Close(void* idna);
std::int32_t NameToASCII(const void* idna, const char16_t* name, std::int32_t length, char16_t* dest,
                         std::int32_t capacity, Info* info, std::int32_t* status);
std::int32_t NameToUnicode(const void* idna, const char16_t* name, std::int32_t length, char16_t* dest,
                           std::int32_t capacity, Info* info, std::int32_t* status);

}
