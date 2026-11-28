#include "prx/libSceIcu/libSceIcu.hpp"
#include <type_traits>

static_assert(std::is_same_v<decltype(&uprv_tzname_clear_cache_nid_postfix), void (APS5_VABI*)()>);

int main() {
    uprv_tzname_clear_cache_nid_postfix();
    uprv_tzname_clear_cache_nid_postfix();
    return 0;
}
