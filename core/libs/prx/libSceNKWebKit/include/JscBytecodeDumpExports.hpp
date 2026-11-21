#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::JscBytecodeDumpAbi {

struct CodeOrigin {
    std::uintptr_t compositeValue;
};

struct OutOfLineCodeOrigin {
    const void* inlineCallFrame;
    std::uint32_t bytecodeIndex;
};

struct CacheableIdentifier {
    std::uintptr_t bits;
};

static_assert(sizeof(CodeOrigin) == 8);
static_assert(offsetof(CodeOrigin, compositeValue) == 0);
static_assert(sizeof(OutOfLineCodeOrigin) == 16);
static_assert(offsetof(OutOfLineCodeOrigin, inlineCallFrame) == 0);
static_assert(offsetof(OutOfLineCodeOrigin, bytecodeIndex) == 8);
static_assert(sizeof(CacheableIdentifier) == 8);
static_assert(offsetof(CacheableIdentifier, bits) == 0);

}

extern "C" {

void APS5_VABI _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(const NKWebKit::JscBytecodeDumpAbi::CodeOrigin* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3JSC19CacheableIdentifier4dumpERN3WTF11PrintStreamE(const NKWebKit::JscBytecodeDumpAbi::CacheableIdentifier* self, NKWebKit::WtfDumpAbi::PrintStream* out);

}
