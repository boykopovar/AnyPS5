#include "prx/libSceIcu/FormattedValueNextPosition.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

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
