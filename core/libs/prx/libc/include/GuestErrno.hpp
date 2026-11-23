#pragma once

#include "general/VabiMacros.hpp"

extern "C" int* APS5_VABI __error_nid_postfix();

inline int FailWithGuestErrno(int error) {
    *__error_nid_postfix() = error;
    return -1;
}
