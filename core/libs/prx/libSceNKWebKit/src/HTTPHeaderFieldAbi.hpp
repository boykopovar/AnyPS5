#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct HTTPHeaderField {
    WtfAbi::String name;
    WtfAbi::String value;
};

static_assert(sizeof(HTTPHeaderField) == 16);
static_assert(offsetof(HTTPHeaderField, name) == 0);
static_assert(offsetof(HTTPHeaderField, value) == 8);

}
