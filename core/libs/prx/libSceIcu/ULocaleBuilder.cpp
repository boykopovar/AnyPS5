#include "prx/libSceIcu/ULocaleBuilder.hpp"
#include "unicode/ulocbuilder.h"

extern "C" ULocaleBuilder* APS5_VABI ulocbld_open_nid_postfix() {
    return ulocbld_open();
}
