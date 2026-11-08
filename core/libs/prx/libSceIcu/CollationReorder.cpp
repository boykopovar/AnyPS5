#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI ucol_getReorderCodes_nid_postfix(const UCollator*, std::int32_t*, std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status)) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
