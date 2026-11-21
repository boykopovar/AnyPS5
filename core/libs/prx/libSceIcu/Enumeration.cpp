#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <cstring>

namespace {

constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t UnsupportedError = 16;
constexpr std::int32_t BufferPad = 8;

bool Failed(const UEnumeration* enumeration, std::int32_t* status) {
    if (!enumeration) return true;
    if (!status) APS5_INVALID_ARG_EX;
    return *status > 0;
}

void* Buffer(UEnumeration* enumeration, std::int32_t capacity) {
    std::int32_t length = 0;
    if (enumeration->baseContext) std::memcpy(&length, enumeration->baseContext, sizeof(length));
    if (!enumeration->baseContext || length < capacity) {
        capacity += BufferPad;
        enumeration->baseContext = realloc_nid_postfix(enumeration->baseContext, sizeof(std::int32_t) + capacity);
        if (!enumeration->baseContext) return nullptr;
        std::memcpy(enumeration->baseContext, &capacity, sizeof(capacity));
    }
    return static_cast<char*>(enumeration->baseContext) + sizeof(std::int32_t);
}

}

extern "C" {

void APS5_VABI uenum_close_nid_postfix(UEnumeration* enumeration) {
    if (!enumeration) return;
    if (!enumeration->close) {
        free_nid_postfix(enumeration);
        return;
    }
    if (enumeration->baseContext) free_nid_postfix(enumeration->baseContext);
    enumeration->close(enumeration);
}

std::int32_t APS5_VABI uenum_count_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (Failed(enumeration, status)) return -1;
    if (enumeration->count) return enumeration->count(enumeration, status);
    *status = UnsupportedError;
    return -1;
}

const char16_t* APS5_VABI uenum_unext_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status)) return nullptr;
    if (enumeration->uNext) return enumeration->uNext(enumeration, resultLength, status);
    *status = UnsupportedError;
    return nullptr;
}

const char* APS5_VABI uenum_next_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status)) return nullptr;
    if (!enumeration->next) {
        *status = UnsupportedError;
        return nullptr;
    }
    std::int32_t ignoredLength = 0;
    return enumeration->next(enumeration, resultLength ? resultLength : &ignoredLength, status);
}

void APS5_VABI uenum_reset_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (Failed(enumeration, status)) return;
    if (enumeration->reset) enumeration->reset(enumeration, status);
    else *status = UnsupportedError;
}

const char16_t* APS5_VABI uenum_unextDefault_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (!enumeration || !status) APS5_INVALID_ARG_EX;
    char16_t* units = nullptr;
    std::int32_t length = 0;
    if (enumeration->next) {
        const char* chars = enumeration->next(enumeration, &length, status);
        if (chars) {
            units = static_cast<char16_t*>(Buffer(enumeration, (length + 1) * static_cast<std::int32_t>(sizeof(char16_t))));
            if (units) u_charsToUChars_nid_postfix(chars, units, length + 1);
            else *status = MemoryAllocationError;
        }
    } else {
        *status = UnsupportedError;
    }
    if (resultLength) *resultLength = length;
    return units;
}

const char* APS5_VABI uenum_nextDefault_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (!enumeration || !resultLength || !status) APS5_INVALID_ARG_EX;
    if (!enumeration->uNext) {
        *status = UnsupportedError;
        return nullptr;
    }
    const char16_t* units = enumeration->uNext(enumeration, resultLength, status);
    if (!units) return nullptr;
    auto* chars = static_cast<char*>(Buffer(enumeration, *resultLength + 1));
    if (!chars) {
        *status = MemoryAllocationError;
        return nullptr;
    }
    u_UCharsToChars_nid_postfix(units, chars, *resultLength + 1);
    return chars;
}

}
