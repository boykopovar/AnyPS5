#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct LegacyProtectionSpaceBase {
    WtfAbi::String host;
    std::int32_t port;
    std::uint32_t serverType;
    WtfAbi::String realm;
    std::uint32_t authenticationScheme;
    bool isHashTableDeletedValue;
};

static_assert(sizeof(LegacyProtectionSpaceBase) == 32);
static_assert(offsetof(LegacyProtectionSpaceBase, host) == 0);
static_assert(offsetof(LegacyProtectionSpaceBase, port) == 8);
static_assert(offsetof(LegacyProtectionSpaceBase, serverType) == 12);
static_assert(offsetof(LegacyProtectionSpaceBase, realm) == 16);
static_assert(offsetof(LegacyProtectionSpaceBase, authenticationScheme) == 24);
static_assert(offsetof(LegacyProtectionSpaceBase, isHashTableDeletedValue) == 28);

}
