#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

USet* APS5_VABI ucol_getTailoredSet_nid_postfix(const UCollator*, std::int32_t* status) {
    if (Icu::Failure(status)) {
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
