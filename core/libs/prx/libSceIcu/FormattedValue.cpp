#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

const char16_t* APS5_VABI ufmtval_getString_nid_postfix(const UFormattedValue* ufmtval, std::int32_t* pLength, std::int32_t* ec) {
    if (Icu::Failure(ec)) return nullptr;
    if (!ufmtval) {
        *ec = Icu::IllegalArgument;
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int8_t APS5_VABI ufmtval_nextPosition_nid_postfix(const UFormattedValue* ufmtval, UConstrainedFieldPosition* ucfpos, std::int32_t* ec) {
    if (Icu::Failure(ec)) return 0;
    if (!ufmtval) {
        *ec = Icu::IllegalArgument;
        return 0;
    }
    if (!ucfpos) {
        *ec = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
