#include "prx/libSceIcu/Identified.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>

extern "C" const char* APS5_VABI locale_getKeywordsStart_nid_postfix(const char* localeID) {
    if (!localeID) APS5_INVALID_ARG_EX;
    return std::strchr(localeID, '@');
}
