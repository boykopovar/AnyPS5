#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"

extern "C" {

void APS5_VABI ucol_setAttribute_nid_postfix(UCollator* coll, UColAttribute, UColAttributeValue, std::int32_t* status) {
    if (Icu::Failure(status) || coll == nullptr) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
