#pragma once

#include "SceTypes.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct Path {
    void* path;
};

static_assert(offsetof(Path, path) == 0);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

bool APS5_VABI _ZNK7WebCore4Path6isNullEv(const NKWebKit::WebCoreAbi::Path* self);

}
