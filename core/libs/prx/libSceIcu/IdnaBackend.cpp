#include "prx/libSceIcu/IdnaBackend.hpp"
#include "unicode/uidna.h"
#include <cstdint>
#include <cstring>

namespace Icu::IdnaBackend {

static_assert(sizeof(Info) == sizeof(UIDNAInfo));

void* OpenUTS46(std::uint32_t options, std::int32_t* status) {
    auto local = static_cast<UErrorCode>(*status);
    void* idna = uidna_openUTS46(options, &local);
    *status = static_cast<std::int32_t>(local);
    return idna;
}

void Close(void* idna) {
    uidna_close(static_cast<UIDNA*>(idna));
}

std::int32_t NameToASCII(const void* idna, const char16_t* name, std::int32_t length, char16_t* dest,
                         std::int32_t capacity, Info* info, std::int32_t* status) {
    UIDNAInfo nativeInfo = UIDNA_INFO_INITIALIZER;
    nativeInfo.size = info->size;
    auto local = static_cast<UErrorCode>(*status);
    const std::int32_t result = uidna_nameToASCII(
        static_cast<const UIDNA*>(idna), name, length, dest, capacity, &nativeInfo, &local);
    std::memcpy(info, &nativeInfo, sizeof(*info));
    *status = static_cast<std::int32_t>(local);
    return result;
}

std::int32_t NameToUnicode(const void* idna, const char16_t* name, std::int32_t length, char16_t* dest,
                           std::int32_t capacity, Info* info, std::int32_t* status) {
    UIDNAInfo nativeInfo = UIDNA_INFO_INITIALIZER;
    nativeInfo.size = info->size;
    auto local = static_cast<UErrorCode>(*status);
    const std::int32_t result = uidna_nameToUnicode(
        static_cast<const UIDNA*>(idna), name, length, dest, capacity, &nativeInfo, &local);
    std::memcpy(info, &nativeInfo, sizeof(*info));
    *status = static_cast<std::int32_t>(local);
    return result;
}

}
