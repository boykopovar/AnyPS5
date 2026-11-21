#pragma once

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WtfAbi {

struct PrintStream {
    void* const* vtable;
};

struct RawPointer {
    const void* value;
};

enum PrintStreamSlot : std::size_t {
    PrintStreamVprintf = 2,
    PrintStreamFlush = 3,
    PrintStreamBegin = 4,
    PrintStreamEnd = 5
};

static_assert(sizeof(PrintStream) == 8);
static_assert(offsetof(PrintStream, vtable) == 0);
static_assert(sizeof(RawPointer) == 8);
static_assert(offsetof(RawPointer, value) == 0);
static_assert(std::is_trivially_copyable_v<RawPointer>);

}  // namespace NKWebKit::WtfAbi
