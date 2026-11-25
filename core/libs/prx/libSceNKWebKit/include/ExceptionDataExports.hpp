#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct ExceptionData {
    std::uint32_t code;
    WtfAbi::String message;
};

static_assert(sizeof(ExceptionData) == 16);
static_assert(alignof(ExceptionData) == 8);
static_assert(offsetof(ExceptionData, code) == 0);
static_assert(offsetof(ExceptionData, message) == 8);

}

extern "C" {

NKWebKit::WebCoreAbi::ExceptionData APS5_VABI _ZNK7WebCore13ExceptionData12isolatedCopyEv(const NKWebKit::WebCoreAbi::ExceptionData* self);

}
