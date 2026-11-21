#pragma once
#include <cstdint>

namespace IcuNumberBackend {

struct Field {
    std::int32_t field, begin, end;
};

struct Parse {
    std::int32_t line, offset;
    char16_t before[16], after[16];
};

void* Open_nid_no_patch(std::int32_t, const char16_t*, std::int32_t, const char*, Parse*, std::int32_t*);
void* Clone_nid_no_patch(const void*, std::int32_t*);
void Close_nid_no_patch(void*);
std::int32_t Double_nid_no_patch(const void*, double, char16_t*, std::int32_t, Field*, std::int32_t*);
std::int32_t Decimal_nid_no_patch(const void*, const char*, std::int32_t, char16_t*, std::int32_t, Field*, std::int32_t*);
std::int32_t Symbol_nid_no_patch(const void*, std::int32_t, char16_t*, std::int32_t, std::int32_t*);
std::int32_t Text_nid_no_patch(const void*, std::int32_t, char16_t*, std::int32_t, std::int32_t*);
void SetAttribute_nid_no_patch(void*, std::int32_t, std::int32_t);
void SetText_nid_no_patch(void*, std::int32_t, const char16_t*, std::int32_t, std::int32_t*);

} // namespace IcuNumberBackend
