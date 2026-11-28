#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Jsc::YarrAbi {

enum class Flags : std::uint16_t {
    HasIndices = 1 << 0,
    Global = 1 << 1,
    IgnoreCase = 1 << 2,
    Multiline = 1 << 3,
    DotAll = 1 << 4,
    Unicode = 1 << 5,
    UnicodeSets = 1 << 6,
    Sticky = 1 << 7,
};

struct OptionalFlags {
    std::uint16_t value;
    bool engaged;
};

static_assert(sizeof(OptionalFlags) == 4);
static_assert(alignof(OptionalFlags) == 2);
static_assert(offsetof(OptionalFlags, value) == 0);
static_assert(offsetof(OptionalFlags, engaged) == 2);
static_assert(std::is_trivially_copyable_v<OptionalFlags>);

} // namespace Jsc::YarrAbi

extern "C" {

Jsc::YarrAbi::OptionalFlags APS5_VABI _ZN3JSC4Yarr10parseFlagsEN3WTF10StringViewE(Jsc::WtfAbi::StringView string);

}
