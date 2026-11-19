#pragma once

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::JscCodeOriginAbi {

inline constexpr unsigned EffectiveAddressWidth = 48;
inline constexpr unsigned FreeBitsAtTop = 64 - EffectiveAddressWidth;
inline constexpr std::uintptr_t MaskIsOutOfLine = 1;
inline constexpr std::uintptr_t MaskIsBytecodeIndexInvalid = 2;
inline constexpr std::uintptr_t MaskCompositeValueForPointer = ((std::uintptr_t{1} << EffectiveAddressWidth) - 1) & ~std::uintptr_t{7};
inline constexpr std::uint32_t CheckpointShift = 2;
inline constexpr std::uint32_t CheckpointMask = 3;

struct CodeOrigin {
    std::uintptr_t compositeValue;
};

struct OutOfLineCodeOrigin {
    const void* inlineCallFrame;
    std::uint32_t bytecodeIndex;
};

static_assert(sizeof(CodeOrigin) == 8);
static_assert(offsetof(OutOfLineCodeOrigin, inlineCallFrame) == 0);
static_assert(offsetof(OutOfLineCodeOrigin, bytecodeIndex) == 8);

}

extern "C" {

void APS5_VABI _ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE(const NKWebKit::JscCodeOriginAbi::CodeOrigin* self, Jsc::WtfAbi::PrintStream* out);

}
