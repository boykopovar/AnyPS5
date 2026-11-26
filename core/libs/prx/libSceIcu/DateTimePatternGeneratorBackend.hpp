#pragma once
#include <cstdint>

namespace IcuPatternBackend {

void* Open_nid_no_patch(const char* locale, std::int32_t* status);
void Close_nid_no_patch(void* handle);
std::int32_t Best_nid_no_patch(void* handle, const char16_t* input, std::int32_t length, std::int32_t options,
                             char16_t* output, std::int32_t capacity, std::int32_t* status);
std::int32_t Skeleton_nid_no_patch(const char16_t* input, std::int32_t length,
                                 char16_t* output, std::int32_t capacity, std::int32_t* status);
std::int32_t Field_nid_no_patch(const void* handle, std::int32_t field, std::int32_t width,
                              char16_t* output, std::int32_t capacity, std::int32_t* status);

} // namespace IcuPatternBackend
