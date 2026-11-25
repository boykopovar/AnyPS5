#pragma once
#include <cstdint>
#include <optional>
#include <vector>

struct UResourceBundle;

namespace IcuResourceBundle {

enum class StringLookup { Direct, Key, Index };

std::optional<bool> OwnedHasNext_nid_no_patch(const UResourceBundle*);
bool CopyOwnedString_nid_no_patch(const UResourceBundle*, StringLookup, const char*, std::int32_t,
                                 std::vector<char16_t>&, std::int32_t&);

} // namespace IcuResourceBundle

namespace IcuResourceBackend {

void* Open_nid_no_patch(const char*, const char*, bool, std::int32_t*);
void Close_nid_no_patch(void*);
void* Child_nid_no_patch(const void*, const char*, std::int32_t, bool, void*, std::int32_t*);
bool HasNext_nid_no_patch(const void*);
std::int32_t Size_nid_no_patch(const void*);
std::int32_t Type_nid_no_patch(const void*);
const char* Key_nid_no_patch(const void*);
const char* Locale_nid_no_patch(const void*, std::int32_t*);
const char16_t* String_nid_no_patch(const void*, std::int32_t, const char*, std::int32_t, std::int32_t*, std::int32_t*);
std::uint32_t UInt_nid_no_patch(const void*, std::int32_t*);
std::int32_t Int_nid_no_patch(const void*, std::int32_t*);
const std::uint8_t* Binary_nid_no_patch(const void*, std::int32_t*, std::int32_t*);
const std::int32_t* Vector_nid_no_patch(const void*, std::int32_t*, std::int32_t*);

} // namespace IcuResourceBackend
