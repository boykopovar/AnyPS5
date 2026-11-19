#pragma once

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::RegisterSetAbi {

struct RegisterSet {
    std::uint64_t bits;
};

inline constexpr unsigned RegisterCount = 32;

static_assert(sizeof(RegisterSet) == 8);
static_assert(offsetof(RegisterSet, bits) == 0);

} // namespace Jsc::RegisterSetAbi

extern "C" {

void APS5_VABI _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE(const Jsc::RegisterSetAbi::RegisterSet* self, Jsc::WtfAbi::PrintStream* out);

}
