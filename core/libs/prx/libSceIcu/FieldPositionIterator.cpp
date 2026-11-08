#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstdint>

extern "C" {

UFieldPositionIterator* APS5_VABI ufieldpositer_open_nid_postfix(std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
