#include "prx/libSceIcu/ListFormatterOpenForType.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

UListFormatter* APS5_VABI ulistfmt_openForType_nid_postfix(const char*, UListFormatterType, UListFormatterWidth,
                                                           std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
