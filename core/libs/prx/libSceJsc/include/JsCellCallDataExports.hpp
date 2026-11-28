#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Jsc::CellAbi {

struct CallData {
    std::uint8_t type;
    void* first;
    void* second;
};

static_assert(sizeof(CallData) == 24);
static_assert(offsetof(CallData, first) == 8);
static_assert(offsetof(CallData, second) == 16);
static_assert(std::is_trivially_copyable_v<CallData>);

} // namespace Jsc::CellAbi

extern "C" {

Jsc::CellAbi::CallData APS5_VABI _ZN3JSC6JSCell11getCallDataEPS0_(void* cell);
Jsc::CellAbi::CallData APS5_VABI _ZN3JSC6JSCell16getConstructDataEPS0_(void* cell);

}
