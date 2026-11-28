#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <mutex>

struct UFieldPositionIterator;

namespace Icu {

std::unique_lock<std::mutex> LockFieldPositionIterator_nid_no_patch(UFieldPositionIterator* iterator, const char* caller);

}

extern "C" {

std::int32_t APS5_VABI ufieldpositer_next_nid_postfix(UFieldPositionIterator*, std::int32_t*, std::int32_t*);
void APS5_VABI ufieldpositer_close_nid_postfix(UFieldPositionIterator*);
UFieldPositionIterator* APS5_VABI ufieldpositer_open_nid_postfix(std::int32_t*);

}
