#pragma once
#include <cstdint>

namespace IcuResourceBackend {

void* Open_nid_no_patch(const char*, const char*, bool, std::int32_t*);
void Close_nid_no_patch(void*);
void* Child_nid_no_patch(const void*, const char*, std::int32_t, bool, void*, std::int32_t*);
std::int32_t Size_nid_no_patch(const void*);
std::int32_t Type_nid_no_patch(const void*);
const char* Key_nid_no_patch(const void*);
const char16_t* String_nid_no_patch(const void*, std::int32_t, const char*, std::int32_t, std::int32_t*, std::int32_t*);
std::int32_t Int_nid_no_patch(const void*, std::int32_t*);
const std::uint8_t* Binary_nid_no_patch(const void*, std::int32_t*, std::int32_t*);
const std::int32_t* Vector_nid_no_patch(const void*, std::int32_t*, std::int32_t*);

} // namespace IcuResourceBackend
