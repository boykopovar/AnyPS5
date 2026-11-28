#pragma once

#include <cstddef>

namespace NKWebKit::WtfDumpAbi {

struct PrintStream {
    void* const* vtable;
};

enum PrintStreamSlot : std::size_t {
    PrintStreamVprintf = 2,
    PrintStreamBegin = 4,
    PrintStreamEnd = 5
};

static_assert(sizeof(PrintStream) == 8);
static_assert(offsetof(PrintStream, vtable) == 0);

}  // namespace NKWebKit::WtfDumpAbi
