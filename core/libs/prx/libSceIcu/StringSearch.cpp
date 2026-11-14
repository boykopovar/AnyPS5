#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

UStringSearch* APS5_VABI usearch_open_nid_postfix(const char16_t*, std::int32_t, const char16_t*, std::int32_t, const char* locale, UBreakIterator*, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    if (locale == nullptr) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
