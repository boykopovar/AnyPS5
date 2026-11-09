#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::UrlAbi {

struct StringView {
    const void* characters;
    std::uint32_t length;
    bool is8Bit;
};

static_assert(sizeof(StringView) == 16);
static_assert(offsetof(StringView, characters) == 0);
static_assert(offsetof(StringView, length) == 8);
static_assert(offsetof(StringView, is8Bit) == 12);

}

extern "C" {

bool APS5_VABI _ZN3WTF14isVersion4UUIDENS_10StringViewE(NKWebKit::UrlAbi::StringView value);
bool APS5_VABI _ZN3WTF24isDefaultPortForProtocolEtNS_10StringViewE(std::uint16_t port, NKWebKit::UrlAbi::StringView protocol);

}
