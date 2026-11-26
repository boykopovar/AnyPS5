#pragma once
#include <cstdint>

namespace NumberFormatOracle {

struct Field {
    std::int32_t field;
    std::int32_t begin;
    std::int32_t end;
};

struct Parse {
    std::int32_t line;
    std::int32_t offset;
    char16_t before[16];
    char16_t after[16];
};

void* Open(std::int32_t, const char16_t*, std::int32_t, const char*, Parse*, std::int32_t*);
void* Clone(const void*, std::int32_t*);
void Close(void*);
std::int32_t Double(const void*, double, char16_t*, std::int32_t, Field*, std::int32_t*);
std::int32_t Decimal(const void*, const char*, std::int32_t, char16_t*, std::int32_t, Field*, std::int32_t*);
std::int32_t Symbol(const void*, std::int32_t, char16_t*, std::int32_t, std::int32_t*);
std::int32_t Text(const void*, std::int32_t, char16_t*, std::int32_t, std::int32_t*);
void SetAttribute(void*, std::int32_t, std::int32_t);
void SetText(void*, std::int32_t, const char16_t*, std::int32_t, std::int32_t*);

}
