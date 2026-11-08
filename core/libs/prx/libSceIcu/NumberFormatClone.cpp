#include "prx/libSceIcu/NumberFormatClone.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

UNumberFormat* APS5_VABI unum_clone_nid_postfix(const UNumberFormat*, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
