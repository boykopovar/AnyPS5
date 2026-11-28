#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string ReadAlias(const char* alias, const char* caller) {
    std::string copy;
    auto address = reinterpret_cast<std::uintptr_t>(alias);
    for (;;) {
        char value = 0;
        if (!GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address), sizeof(value)))
            NotImplemented_nid_no_patch(caller);
        if (value == 0) return copy;
        if (copy.size() < UCNV_MAX_CONVERTER_NAME_LENGTH) copy.push_back(value);
        if (address == std::numeric_limits<std::uintptr_t>::max()) NotImplemented_nid_no_patch(caller);
        ++address;
    }
}

void WriteStatus(std::int32_t* error, UErrorCode status, std::int32_t initial, const char* caller) {
    const auto result = static_cast<std::int32_t>(status);
    if (result != initial && !GuestCopyOut_nid_no_patch(error, &result, sizeof(result))) NotImplemented_nid_no_patch(caller);
}

}

extern "C" void APS5_VABI ucnv_getAliases_nid_postfix(const char* alias, const char** aliases,
                                                       std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    std::int32_t initial = 0;
    if (!GuestCopyIn_nid_no_patch(&initial, error, sizeof(initial))) NotImplemented_nid_no_patch(__func__);
    UErrorCode status = static_cast<UErrorCode>(initial);
    if (U_FAILURE(status)) return;
    if (!GuestWritable_nid_no_patch(error, sizeof(*error))) NotImplemented_nid_no_patch(__func__);

    ucnv_countAliases("", &status);
    if (U_FAILURE(status)) {
        WriteStatus(error, status, initial, __func__);
        return;
    }

    std::string aliasCopy;
    const char* nativeAlias = nullptr;
    if (alias) {
        aliasCopy = ReadAlias(alias, __func__);
        nativeAlias = aliasCopy.c_str();
    }
    const auto count = ucnv_countAliases(nativeAlias, &status);
    if (U_FAILURE(status) || count == 0) {
        WriteStatus(error, status, initial, __func__);
        return;
    }
    const std::size_t bytes = static_cast<std::size_t>(count) * sizeof(*aliases);
    if (!aliases || !GuestWritable_nid_no_patch(aliases, bytes)) NotImplemented_nid_no_patch(__func__);
    std::vector<const char*> nativeAliases(count);
    ucnv_getAliases(nativeAlias, nativeAliases.data(), &status);
    if (!GuestCopyOut_nid_no_patch(aliases, nativeAliases.data(), bytes)) NotImplemented_nid_no_patch(__func__);
    WriteStatus(error, status, initial, __func__);
}

namespace {

bool BeginStatus(std::int32_t* error, std::int32_t& initial, const char* caller) {
    if (!error) throw std::invalid_argument(std::string(caller) + ": invalid argument");
    if (!GuestCopyIn_nid_no_patch(&initial, error, sizeof(initial))) NotImplemented_nid_no_patch(caller);
    if (U_FAILURE(static_cast<UErrorCode>(initial))) return false;
    if (!GuestWritable_nid_no_patch(error, sizeof(*error))) NotImplemented_nid_no_patch(caller);
    return true;
}

}

extern "C" std::int32_t APS5_VABI ucnv_countAvailable_nid_postfix() {
    return ucnv_countAvailable();
}

extern "C" const char* APS5_VABI ucnv_getAvailableName_nid_postfix(std::int32_t index) {
    return ucnv_getAvailableName(index);
}

extern "C" std::uint16_t APS5_VABI ucnv_countStandards_nid_postfix() {
    return ucnv_countStandards();
}

extern "C" std::uint16_t APS5_VABI ucnv_countAliases_nid_postfix(const char* alias, std::int32_t* error) {
    std::int32_t initial = 0;
    if (!BeginStatus(error, initial, __func__)) return 0;
    std::string aliasCopy;
    if (alias) aliasCopy = ReadAlias(alias, __func__);
    UErrorCode status = static_cast<UErrorCode>(initial);
    const auto count = ucnv_countAliases(alias ? aliasCopy.c_str() : nullptr, &status);
    WriteStatus(error, status, initial, __func__);
    return count;
}

extern "C" const char* APS5_VABI ucnv_getAlias_nid_postfix(const char* alias, std::uint16_t index,
                                                           std::int32_t* error) {
    std::int32_t initial = 0;
    if (!BeginStatus(error, initial, __func__)) return nullptr;
    std::string aliasCopy;
    if (alias) aliasCopy = ReadAlias(alias, __func__);
    UErrorCode status = static_cast<UErrorCode>(initial);
    const char* result = ucnv_getAlias(alias ? aliasCopy.c_str() : nullptr, index, &status);
    WriteStatus(error, status, initial, __func__);
    return result;
}

extern "C" const char* APS5_VABI ucnv_getStandardName_nid_postfix(const char* name, const char* standard,
                                                                  std::int32_t* error) {
    std::int32_t initial = 0;
    if (!BeginStatus(error, initial, __func__)) return nullptr;
    std::string nameCopy;
    std::string standardCopy;
    if (name) nameCopy = ReadAlias(name, __func__);
    if (standard) standardCopy = ReadAlias(standard, __func__);
    UErrorCode status = static_cast<UErrorCode>(initial);
    const char* result = ucnv_getStandardName(name ? nameCopy.c_str() : nullptr,
                                              standard ? standardCopy.c_str() : nullptr, &status);
    WriteStatus(error, status, initial, __func__);
    return result;
}
