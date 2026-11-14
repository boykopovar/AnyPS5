#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct ContentType {
    WtfAbi::String type;
};

static_assert(sizeof(ContentType) == 8);
static_assert(offsetof(ContentType, type) == 0);

}
