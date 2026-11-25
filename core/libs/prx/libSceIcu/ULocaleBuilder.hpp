#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"

struct ULocaleBuilder;

extern "C" {
ULocaleBuilder* APS5_VABI ulocbld_open_nid_postfix();
}
