#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::ExceptionAbi {

struct ExceptionData {
    int code;
    WtfAbi::String message;
};

static_assert(offsetof(ExceptionData, code) == 0);
static_assert(offsetof(ExceptionData, message) == 8);
static_assert(sizeof(ExceptionData) == 16);
static_assert(!std::is_trivially_copyable_v<ExceptionData>);

}  // namespace NKWebKit::ExceptionAbi

extern "C" {

NKWebKit::ExceptionAbi::ExceptionData APS5_VABI _ZNK7WebCore13ExceptionData12isolatedCopyEv(const NKWebKit::ExceptionAbi::ExceptionData* self);

}
