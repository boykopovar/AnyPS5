#pragma once

#include "prx/libSceNKWebKit/src/HTTPHeaderFieldAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct CustomHeaderFields {
    WtfAbi::Vector<HTTPHeaderField> fields;
    WtfAbi::Vector<WtfAbi::String> thirdPartyDomains;
};

static_assert(sizeof(CustomHeaderFields) == 32);
static_assert(offsetof(CustomHeaderFields, fields) == 0);
static_assert(offsetof(CustomHeaderFields, thirdPartyDomains) == 16);
static_assert(std::is_standard_layout_v<CustomHeaderFields>);

}  // namespace NKWebKit::WebCoreAbi
