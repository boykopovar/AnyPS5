#pragma once
#include <cstdint>

namespace IcuCalendarBackend {

void* Open_nid_no_patch(const char16_t*, std::int32_t, const char*, std::int32_t, std::int32_t*);
void* Clone_nid_no_patch(const void*, std::int32_t*);
void Close_nid_no_patch(void*);
std::int32_t Get_nid_no_patch(const void*, std::int32_t, std::int32_t*);
void Set_nid_no_patch(void*, double, bool, std::int32_t*);

} // namespace IcuCalendarBackend
