#include "prx/libSceNKWebKit/include/InventoryExports.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

std::uint32_t CodeUnit(NKWebKit::StringView value, std::uint32_t index) {
    return value.is8Bit ? static_cast<const std::uint8_t*>(value.data)[index] : static_cast<const char16_t*>(value.data)[index];
}

bool EqualsIgnoringAsciiCase(std::uint32_t codeUnit, char expected) {
    if (codeUnit >= 'A' && codeUnit <= 'Z') codeUnit += 'a' - 'A';
    return codeUnit == static_cast<unsigned char>(expected);
}

void Validate(NKWebKit::StringView value) {
    if (!value.data && value.length) throw std::invalid_argument("WTF protocol predicate: null characters with nonzero length");
}

}

extern "C" {

bool APS5_VABI _ZN3WTF20protocolIsJavaScriptENS_10StringViewE(NKWebKit::StringView value) {
    Validate(value);
    constexpr char protocol[] = "javascript";
    bool leading = true;
    std::size_t expectedIndex = 0;
    for (std::uint32_t index = 0; index < value.length; ++index) {
        const std::uint32_t codeUnit = CodeUnit(value, index);
        if (leading) {
            if (codeUnit <= ' ') continue;
            leading = false;
        } else if (codeUnit == '\t' || codeUnit == '\r' || codeUnit == '\n') {
            continue;
        }
        if (!protocol[expectedIndex]) return codeUnit == ':';
        if (!EqualsIgnoringAsciiCase(codeUnit, protocol[expectedIndex])) return false;
        ++expectedIndex;
    }
    return false;
}

bool APS5_VABI _ZN3WTF22protocolIsInHTTPFamilyENS_10StringViewE(NKWebKit::StringView value) {
    Validate(value);
    if (value.length < 5) return false;
    constexpr char prefix[] = "http";
    for (std::uint32_t index = 0; index < 4; ++index) {
        if (!EqualsIgnoringAsciiCase(CodeUnit(value, index), prefix[index])) return false;
    }
    if (CodeUnit(value, 4) == ':') return true;
    return EqualsIgnoringAsciiCase(CodeUnit(value, 4), 's') && value.length >= 6 && CodeUnit(value, 5) == ':';
}

}
