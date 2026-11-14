#include "prx/libSceIcu/TimeZone.hpp"
#include "prx/libc/include/Api.hpp"

extern "C" void APS5_VABI uprv_tzset_nid_postfix() {
    tzset_nid_postfix();
}
