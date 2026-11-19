#pragma once

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::JscCellAttributesAbi {

enum DestructionMode : std::int8_t {
    DoesNotNeedDestruction,
    NeedsDestruction,
};

enum HeapCellKind : std::int8_t {
    JSCell,
    JSCellWithIndexingHeader,
    Auxiliary,
};

struct CellAttributes {
    DestructionMode destruction;
    HeapCellKind cellKind;
};

static_assert(sizeof(CellAttributes) == 2);
static_assert(alignof(CellAttributes) == 1);
static_assert(offsetof(CellAttributes, destruction) == 0);
static_assert(offsetof(CellAttributes, cellKind) == 1);

}

extern "C" {

void APS5_VABI _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(const NKWebKit::JscCellAttributesAbi::CellAttributes* self, Jsc::WtfAbi::PrintStream* out);

}
