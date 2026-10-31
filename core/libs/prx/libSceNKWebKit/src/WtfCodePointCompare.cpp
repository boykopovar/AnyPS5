#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;

StringView View(const String& value) {
    if (!value.impl) return {nullptr, 0, true};
    return {value.impl->data, value.impl->length, (value.impl->hashAndFlags & 4) != 0};
}

unsigned Unit(StringView value, unsigned index) {
    return value.is8Bit ? static_cast<const unsigned char*>(value.data)[index]
                       : static_cast<const char16_t*>(value.data)[index];
}

int Compare(StringView first, StringView second) {
    const unsigned commonLength = std::min(first.length, second.length);
    if (commonLength && (!first.data || !second.data)) {
        throw std::invalid_argument("null WTF comparison buffer");
    }
    for (unsigned index = 0; index < commonLength; ++index) {
        const unsigned firstUnit = Unit(first, index);
        const unsigned secondUnit = Unit(second, index);
        if (firstUnit < secondUnit) return -1;
        if (firstUnit > secondUnit) return 1;
    }
    return (first.length > second.length) - (first.length < second.length);
}

}

extern "C" {

int APS5_VABI _ZN3WTF16codePointCompareENS_10StringViewES0_(StringView first, StringView second) {
    return Compare(first, second);
}

int APS5_VABI _ZN3WTF16codePointCompareERKNS_6StringES2_(const String& first, const String& second) {
    return Compare(View(first), View(second));
}

}
