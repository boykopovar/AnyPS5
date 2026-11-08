#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

UDateIntervalFormat* APS5_VABI udtitvfmt_open_nid_postfix(const char*, const char16_t* skeleton, std::int32_t skeletonLength, const char16_t* tzID, std::int32_t tzIDLength, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    if ((!skeleton ? skeletonLength != 0 : skeletonLength < -1) || (!tzID ? tzIDLength != 0 : tzIDLength < -1)) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
