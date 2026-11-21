#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>

namespace Jsc::WtfAbi {

struct LoggerObserverVector {
    void** buffer;
    unsigned capacity;
    unsigned size;
};

static_assert(sizeof(LoggerObserverVector) == 16);
static_assert(alignof(LoggerObserverVector) == 8);
static_assert(offsetof(LoggerObserverVector, buffer) == 0);
static_assert(offsetof(LoggerObserverVector, capacity) == 8);
static_assert(offsetof(LoggerObserverVector, size) == 12);

} // namespace Jsc::WtfAbi

extern "C" {

Jsc::WtfAbi::LoggerObserverVector* APS5_VABI _ZN3WTF6Logger9observersEv();

}
