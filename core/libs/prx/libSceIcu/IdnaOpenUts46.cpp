#include "prx/libSceIcu/IdnaOpenUts46.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

UIDNA* APS5_VABI uidna_openUTS46_nid_postfix(std::uint32_t, std::int32_t* pErrorCode) {
    if (Icu::Failure(pErrorCode)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
