#pragma once

#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WtfAbi {

using PrintStream = WtfDumpAbi::PrintStream;
using WtfDumpAbi::PrintStreamBegin;
using WtfDumpAbi::PrintStreamEnd;
using WtfDumpAbi::PrintStreamVprintf;

struct RawPointer {
    const void* value;
};

static_assert(sizeof(RawPointer) == 8);
static_assert(offsetof(RawPointer, value) == 0);
static_assert(std::is_trivially_copyable_v<RawPointer>);

}  // namespace NKWebKit::WtfAbi
