#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::JscDumpAbi {

struct CellAttributes {
    std::int8_t destruction;
    std::int8_t cellKind;
};

struct HeapRange {
    std::uint16_t begin;
    std::uint16_t end;
};

static_assert(sizeof(CellAttributes) == 2);
static_assert(offsetof(CellAttributes, destruction) == 0);
static_assert(offsetof(CellAttributes, cellKind) == 1);
static_assert(sizeof(HeapRange) == 4);
static_assert(offsetof(HeapRange, begin) == 0);
static_assert(offsetof(HeapRange, end) == 2);

}

extern "C" {

void APS5_VABI _ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE(const NKWebKit::JscDumpAbi::CellAttributes* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE(const NKWebKit::JscDumpAbi::HeapRange* self, NKWebKit::WtfDumpAbi::PrintStream* out);

}
