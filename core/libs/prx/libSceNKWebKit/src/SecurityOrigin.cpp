#include "prx/libSceNKWebKit/include/SecurityOriginExports.hpp"

#include <cstddef>

namespace {

using NKWebKit::WtfAbi::StringView;

char16_t Unit(const StringView& view, std::size_t index) {
    if (view.is8Bit) return static_cast<const unsigned char*>(view.data)[index];
    return static_cast<const char16_t*>(view.data)[index];
}

char16_t Lower(char16_t value) {
    return value >= u'A' && value <= u'Z' ? static_cast<char16_t>(value + (u'a' - u'A')) : value;
}

bool MatchesAt(const StringView& view, std::size_t offset, const char* text, std::size_t length, bool ignoreCase) {
    for (std::size_t i = 0; i < length; ++i) {
        char16_t unit = Unit(view, offset + i);
        if (ignoreCase) unit = Lower(unit);
        if (unit != static_cast<unsigned char>(text[i])) return false;
    }
    return true;
}

bool IsLoopbackIPAddress(const StringView& host) {
    if (host.length == 5 && MatchesAt(host, 0, "[::1]", 5, false)) return true;
    if (host.length < 4 || !MatchesAt(host, 0, "127.", 4, false)) return false;
    std::size_t dots = 0;
    for (std::size_t i = 0; i < host.length; ++i) {
        const char16_t unit = Unit(host, i);
        if (unit == u'.') {
            ++dots;
            continue;
        }
        if (unit < u'0' || unit > u'9') return false;
    }
    return dots == 3;
}

}

extern "C" {

bool APS5_VABI _ZN7WebCore14SecurityOrigin30isLocalHostOrLoopbackIPAddressEN3WTF10StringViewE(StringView host) {
    if (!host.data) host.length = 0;
    if (IsLoopbackIPAddress(host)) return true;
    if (host.length == 9 && MatchesAt(host, 0, "localhost", 9, true)) return true;
    constexpr std::size_t suffixLength = 10;
    return host.length >= suffixLength && MatchesAt(host, host.length - suffixLength, ".localhost", suffixLength, true);
}

}
