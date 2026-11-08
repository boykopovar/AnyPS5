#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UFieldPositionIterator;

extern "C" {
UFieldPositionIterator* APS5_VABI ufieldpositer_open_nid_postfix(std::int32_t*);
}
