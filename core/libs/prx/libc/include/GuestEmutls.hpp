#pragma once

#include <cstddef>
#include <cstdint>

struct GuestEmutlsControl {
    std::uintptr_t size;
    std::uintptr_t alignment;
    std::uintptr_t index;
    const void* initialValue;
};

static_assert(sizeof(GuestEmutlsControl) == 32);
static_assert(offsetof(GuestEmutlsControl, index) == 16);
static_assert(offsetof(GuestEmutlsControl, initialValue) == 24);
