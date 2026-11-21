#include "prx/libSceIcu/tests/ResourceBundleOracle.hpp"
#include "unicode/ures.h"
#include "unicode/udata.h"
#include "unicode/uclean.h"
#include <type_traits>

static_assert(sizeof(UErrorCode) == 4 && sizeof(UResType) == 4);
static_assert(std::is_same_v<UChar, char16_t>);
static_assert(URES_STRING == 0 && URES_BINARY == 1 && URES_TABLE == 2 && URES_INT == 7 && URES_ARRAY == 8 && URES_INT_VECTOR == 14);

void ResourceOracleRegister(const void* data, std::int32_t* status) {
    auto error = static_cast<UErrorCode>(*status);
    udata_setCommonData(data, &error);
    if (U_SUCCESS(error)) udata_setFileAccess(UDATA_NO_FILES, &error);
    *status = error;
}
void ResourceOracleCleanup() { u_cleanup(); }
std::int32_t ResourceOracleNoFiles() { return UDATA_NO_FILES; }
void* ResourceOracleOpen(const char* path, const char* name, bool direct, std::int32_t* status) {
    auto error = status ? static_cast<UErrorCode>(*status) : U_ZERO_ERROR;
    auto* result = direct ? ures_openDirect(path, name, status ? &error : nullptr) : ures_open(path, name, status ? &error : nullptr);
    if (status) *status = error;
    return result;
}
void ResourceOracleClose(void* handle) { ures_close(static_cast<UResourceBundle*>(handle)); }
void* ResourceOracleChild(const void* handle, const char* key, std::int32_t index, bool byKey, void* fillIn, std::int32_t* status) {
    auto error = status ? static_cast<UErrorCode>(*status) : U_ZERO_ERROR;
    auto* result = byKey ? ures_getByKey(static_cast<const UResourceBundle*>(handle), key, static_cast<UResourceBundle*>(fillIn), status ? &error : nullptr)
        : ures_getByIndex(static_cast<const UResourceBundle*>(handle), index, static_cast<UResourceBundle*>(fillIn), status ? &error : nullptr);
    if (status) *status = error;
    return result;
}
std::int32_t ResourceOracleSize(const void* handle) { return ures_getSize(static_cast<const UResourceBundle*>(handle)); }
std::int32_t ResourceOracleType(const void* handle) { return ures_getType(static_cast<const UResourceBundle*>(handle)); }
const char* ResourceOracleKey(const void* handle) { return ures_getKey(static_cast<const UResourceBundle*>(handle)); }
const char16_t* ResourceOracleString(const void* handle, std::int32_t mode, const char* key, std::int32_t index, std::int32_t* length, std::int32_t* status) {
    auto error = status ? static_cast<UErrorCode>(*status) : U_ZERO_ERROR;
    const auto* source = static_cast<const UResourceBundle*>(handle);
    auto* target = status ? &error : nullptr;
    const char16_t* result;
    if (mode == 0) result = ures_getString(source, length, target);
    else if (mode == 1) result = ures_getStringByKey(source, key, length, target);
    else result = ures_getStringByIndex(source, index, length, target);
    if (status) *status = error;
    return result;
}
std::int32_t ResourceOracleInt(const void* handle, std::int32_t* status) {
    auto error = status ? static_cast<UErrorCode>(*status) : U_ZERO_ERROR;
    const auto result = ures_getInt(static_cast<const UResourceBundle*>(handle), status ? &error : nullptr);
    if (status) *status = error;
    return result;
}
const std::uint8_t* ResourceOracleBinary(const void* handle, std::int32_t* length, std::int32_t* status) {
    auto error = status ? static_cast<UErrorCode>(*status) : U_ZERO_ERROR;
    const auto* result = ures_getBinary(static_cast<const UResourceBundle*>(handle), length, status ? &error : nullptr);
    if (status) *status = error;
    return result;
}
const std::int32_t* ResourceOracleVector(const void* handle, std::int32_t* length, std::int32_t* status) {
    auto error = status ? static_cast<UErrorCode>(*status) : U_ZERO_ERROR;
    const auto* result = ures_getIntVector(static_cast<const UResourceBundle*>(handle), length, status ? &error : nullptr);
    if (status) *status = error;
    return result;
}
