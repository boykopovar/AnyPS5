#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct HTTPHeaderField {
    WtfAbi::String name;
    WtfAbi::String value;
};

static_assert(sizeof(HTTPHeaderField) == 16);
static_assert(offsetof(HTTPHeaderField, name) == 0);
static_assert(offsetof(HTTPHeaderField, value) == 8);

struct OptionalHTTPHeaderField {
    bool hasValue{false};
    HTTPHeaderField field;
};

static_assert(sizeof(OptionalHTTPHeaderField) == 24);
static_assert(alignof(OptionalHTTPHeaderField) == 8);
static_assert(offsetof(OptionalHTTPHeaderField, hasValue) == 0);
static_assert(offsetof(OptionalHTTPHeaderField, field) == 8);
static_assert(!std::is_trivially_destructible_v<OptionalHTTPHeaderField>);

}
