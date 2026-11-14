#include "prx/libSceIcu/NumberingSystemBackend.hpp"
#include "unicode/unumsys.h"
#include "unicode/uenum.h"
#include <type_traits>

static_assert(sizeof(UErrorCode) == 4 && sizeof(UBool) == 1);
static_assert(std::is_same_v<UChar, char16_t>);

namespace IcuNumberingBackend {

void* Open_nid_no_patch(const char* name, bool byName, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    auto* result = byName ? unumsys_openByName(name, &error) : unumsys_open(name, &error);
    *status = error;
    return result;
}

void Close_nid_no_patch(void* handle) { unumsys_close(static_cast<UNumberingSystem*>(handle)); }
const char* Name_nid_no_patch(const void* handle) { return unumsys_getName(static_cast<const UNumberingSystem*>(handle)); }
std::int8_t Algorithmic_nid_no_patch(const void* handle) { return unumsys_isAlgorithmic(static_cast<const UNumberingSystem*>(handle)); }
void* Enumeration_nid_no_patch(std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    auto* result = unumsys_openAvailableNames(&error);
    *status = error;
    return result;
}
void EnumClose_nid_no_patch(void* handle) { uenum_close(static_cast<UEnumeration*>(handle)); }
std::int32_t Count_nid_no_patch(void* handle, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto result = uenum_count(static_cast<UEnumeration*>(handle), &error);
    *status = error;
    return result;
}
const char* Next_nid_no_patch(void* handle, std::int32_t* length, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto* result = uenum_next(static_cast<UEnumeration*>(handle), length, &error);
    *status = error;
    return result;
}
const char16_t* UNext_nid_no_patch(void* handle, std::int32_t* length, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto* result = uenum_unext(static_cast<UEnumeration*>(handle), length, &error);
    *status = error;
    return result;
}
void Reset_nid_no_patch(void* handle, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    uenum_reset(static_cast<UEnumeration*>(handle), &error);
    *status = error;
}

} // namespace IcuNumberingBackend
