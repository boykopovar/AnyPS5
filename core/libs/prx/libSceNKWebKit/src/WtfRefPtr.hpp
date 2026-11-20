#pragma once

#include <cstddef>
#include <cstdint>

struct CairoSurface;

namespace NKWebKit::WtfAbi {

template<typename T>
struct RefPtr {
    T* ptr{nullptr};
};

static_assert(sizeof(RefPtr<CairoSurface>) == 8);
static_assert(alignof(RefPtr<CairoSurface>) == 8);
static_assert(offsetof(RefPtr<CairoSurface>, ptr) == 0);

}  // namespace NKWebKit::WtfAbi
