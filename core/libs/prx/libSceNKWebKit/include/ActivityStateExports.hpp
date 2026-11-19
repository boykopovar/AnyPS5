#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct ActivityStateFlags {
    std::uint32_t storage;
};

static_assert(sizeof(ActivityStateFlags) == 4);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS0_9OptionSetINS_13ActivityState4FlagEEE(NKWebKit::WtfAbi::TextStream& stream, NKWebKit::WebCoreAbi::ActivityStateFlags flags);

}
