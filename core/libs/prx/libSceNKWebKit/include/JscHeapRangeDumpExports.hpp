#pragma once

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::DomJitAbi {

struct HeapRange {
    std::uint16_t begin;
    std::uint16_t end;
};

static_assert(sizeof(HeapRange) == 4);
static_assert(offsetof(HeapRange, begin) == 0);
static_assert(offsetof(HeapRange, end) == 2);

}

extern "C" {

void APS5_VABI _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(const NKWebKit::DomJitAbi::HeapRange* self, Jsc::WtfAbi::PrintStream* out);

}
