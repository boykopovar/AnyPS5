#pragma once
#include <cstdint>

namespace IcuNumberingBackend {

void* Open_nid_no_patch(const char*, bool, std::int32_t*);
void Close_nid_no_patch(void*);
const char* Name_nid_no_patch(const void*);
std::int8_t Algorithmic_nid_no_patch(const void*);
void* Enumeration_nid_no_patch(std::int32_t*);
void EnumClose_nid_no_patch(void*);
std::int32_t Count_nid_no_patch(void*, std::int32_t*);
const char* Next_nid_no_patch(void*, std::int32_t*, std::int32_t*);
const char16_t* UNext_nid_no_patch(void*, std::int32_t*, std::int32_t*);
void Reset_nid_no_patch(void*, std::int32_t*);

} // namespace IcuNumberingBackend
