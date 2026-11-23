#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>

namespace NKWebKit::MediaAbi {

struct MediaTimeRange {
    MediaTime start;
    MediaTime end;
};

static_assert(sizeof(MediaTimeRange) == 32);
static_assert(alignof(MediaTimeRange) == 8);
static_assert(offsetof(MediaTimeRange, start) == 0);
static_assert(offsetof(MediaTimeRange, end) == 16);

}

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF14MediaTimeRange12toJSONStringEv(const NKWebKit::MediaAbi::MediaTimeRange* self);

}
