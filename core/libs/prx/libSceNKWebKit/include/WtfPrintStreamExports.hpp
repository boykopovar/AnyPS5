#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <cstddef>
#include <cstdint>

namespace NKWebKit::WtfAbi {

using PrintStream = Jsc::WtfAbi::PrintStream;
using PrintStreamSlot = Jsc::WtfAbi::PrintStreamSlot;

void WtfPrintUtf8_nid_no_patch(PrintStream& out, const StringView& view, const char* type);
void WtfPrintUtf8Atomically_nid_no_patch(PrintStream& out, const StringView& view, const char* type);

}

namespace JSC {

enum class TypedArrayMode : std::uint8_t {
    FastTypedArray = 0b0001'0000,
    OversizeTypedArray = 0b0011'0000,
    WastefulTypedArray = 0b0101'1000,
    GrowableSharedWastefulTypedArray = 0b0101'1010,
    GrowableSharedAutoLengthWastefulTypedArray = 0b0101'1011,
    ResizableNonSharedWastefulTypedArray = 0b0101'1100,
    ResizableNonSharedAutoLengthWastefulTypedArray = 0b0101'1101,
    DataViewMode = 0b1000'1000,
    GrowableSharedDataViewMode = 0b1000'1010,
    GrowableSharedAutoLengthDataViewMode = 0b1000'1011,
    ResizableNonSharedDataViewMode = 0b1000'1100,
    ResizableNonSharedAutoLengthDataViewMode = 0b1000'1101,
};

}

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(NKWebKit::WtfAbi::PrintStream& out, JSC::TypedArrayMode mode);
void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(NKWebKit::WtfAbi::PrintStream& out, const NKWebKit::WtfAbi::StringImpl* string);

}
