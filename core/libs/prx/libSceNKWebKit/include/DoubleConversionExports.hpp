#pragma once

#include "SceTypes.hpp"

#include <cstddef>

namespace NKWebKit::WtfAbi {

struct DoubleToStringConverter {
    int flags;
    const char* infinitySymbol;
    const char* nanSymbol;
    char exponentCharacter;
    int decimalInShortestLow;
    int decimalInShortestHigh;
    int maxLeadingPaddingZeroesInPrecisionMode;
    int maxTrailingPaddingZeroesInPrecisionMode;
};

static_assert(sizeof(DoubleToStringConverter) == 48);
static_assert(offsetof(DoubleToStringConverter, flags) == 0);
static_assert(offsetof(DoubleToStringConverter, infinitySymbol) == 8);
static_assert(offsetof(DoubleToStringConverter, nanSymbol) == 16);
static_assert(offsetof(DoubleToStringConverter, exponentCharacter) == 24);
static_assert(offsetof(DoubleToStringConverter, decimalInShortestLow) == 28);
static_assert(offsetof(DoubleToStringConverter, decimalInShortestHigh) == 32);
static_assert(offsetof(DoubleToStringConverter, maxLeadingPaddingZeroesInPrecisionMode) == 36);
static_assert(offsetof(DoubleToStringConverter, maxTrailingPaddingZeroesInPrecisionMode) == 40);

struct DoubleConversionStringBuilder {
    struct BufferReference {
        char* start;
        int length;
    };
    BufferReference buffer;
    int position;
};

static_assert(sizeof(DoubleConversionStringBuilder::BufferReference) == 16);
static_assert(sizeof(DoubleConversionStringBuilder) == 24);
static_assert(offsetof(DoubleConversionStringBuilder, buffer) == 0);
static_assert(offsetof(DoubleConversionStringBuilder, position) == 16);

}  // namespace NKWebKit::WtfAbi

extern "C" {

const NKWebKit::WtfAbi::DoubleToStringConverter* APS5_VABI
_ZN3WTF17double_conversion23DoubleToStringConverter19EcmaScriptConverterEv();
bool APS5_VABI
_ZNK3WTF17double_conversion23DoubleToStringConverter13ToExponentialEdiPNS0_13StringBuilderE(
    const NKWebKit::WtfAbi::DoubleToStringConverter* self,
    double value,
    int requestedDigits,
    NKWebKit::WtfAbi::DoubleConversionStringBuilder* resultBuilder);

}
