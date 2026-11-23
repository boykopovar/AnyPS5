#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct LegacyOriginPort {
    bool engaged;
    std::uint16_t value;
};

struct LegacySecurityOriginTuple {
    WtfAbi::StringImpl* protocol;
    WtfAbi::StringImpl* host;
    LegacyOriginPort port;
};

struct LegacyOptionalSecurityOriginTuple {
    bool engaged;
    LegacySecurityOriginTuple value;
};

static_assert(sizeof(LegacyOriginPort) == 4);
static_assert(offsetof(LegacyOriginPort, value) == 2);
static_assert(sizeof(LegacySecurityOriginTuple) == 24);
static_assert(offsetof(LegacySecurityOriginTuple, host) == 8);
static_assert(offsetof(LegacySecurityOriginTuple, port) == 16);
static_assert(sizeof(LegacyOptionalSecurityOriginTuple) == 32);
static_assert(offsetof(LegacyOptionalSecurityOriginTuple, value) == 8);
static_assert(std::is_trivially_copyable_v<LegacyOptionalSecurityOriginTuple>);

}  // namespace NKWebKit::WebCoreAbi
