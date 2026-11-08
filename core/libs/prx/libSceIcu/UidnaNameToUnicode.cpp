#include "prx/libSceIcu/UidnaNameToUnicode.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

struct InfoSize {
    std::int16_t size;
};

}

extern "C" {

std::int32_t APS5_VABI uidna_nameToUnicode_nid_postfix(const UIDNA*, const char16_t* name, std::int32_t length, char16_t* dest,
                             std::int32_t capacity, UIDNAInfo* pInfo, std::int32_t* pErrorCode) {
    if (Icu::Failure(pErrorCode)) return 0;
    if (!pInfo || reinterpret_cast<const InfoSize*>(pInfo)->size < 16) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    if ((name == nullptr) ? length != 0 : length < -1) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    if ((dest == nullptr) ? capacity != 0 : capacity < 0) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    if (dest == name && name != nullptr) {
        *pErrorCode = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
