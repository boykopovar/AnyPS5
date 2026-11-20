#include "prx/libSceIcu/tests/NumberingSystemOracle.hpp"
#include "unicode/unumsys.h"
#include "unicode/uenum.h"
#include <type_traits>

static_assert(sizeof(UErrorCode) == 4 && sizeof(UBool) == 1);
static_assert(std::is_same_v<UChar, char16_t>);

void* NumberingOracleOpen(const char* name, bool byName, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    auto* result = byName ? unumsys_openByName(name, &error) : unumsys_open(name, &error);
    *status = error;
    return result;
}

void NumberingOracleClose(void* handle) { unumsys_close(static_cast<UNumberingSystem*>(handle)); }
const char* NumberingOracleName(const void* handle) { return unumsys_getName(static_cast<const UNumberingSystem*>(handle)); }
std::int8_t NumberingOracleAlgorithmic(const void* handle) { return unumsys_isAlgorithmic(static_cast<const UNumberingSystem*>(handle)); }
void* NumberingOracleEnumeration(std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    auto* result = unumsys_openAvailableNames(&error);
    *status = error;
    return result;
}
void NumberingOracleEnumClose(void* handle) { uenum_close(static_cast<UEnumeration*>(handle)); }
std::int32_t NumberingOracleCount(void* handle, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto result = uenum_count(static_cast<UEnumeration*>(handle), &error);
    *status = error;
    return result;
}
const char* NumberingOracleNext(void* handle, std::int32_t* length, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto* result = uenum_next(static_cast<UEnumeration*>(handle), length, &error);
    *status = error;
    return result;
}
const char16_t* NumberingOracleUNext(void* handle, std::int32_t* length, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    const auto* result = uenum_unext(static_cast<UEnumeration*>(handle), length, &error);
    *status = error;
    return result;
}
void NumberingOracleReset(void* handle, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    uenum_reset(static_cast<UEnumeration*>(handle), &error);
    *status = error;
}
