#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

constexpr std::int32_t UcolDefault = -1;

}

extern "C" {

std::int32_t APS5_VABI ucol_getAttribute_nid_postfix(const UCollator* coll, std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status) || !coll) {
        return UcolDefault;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
