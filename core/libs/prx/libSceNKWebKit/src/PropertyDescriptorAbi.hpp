#pragma once

#include <cstddef>
#include <cstdint>

namespace NKWebKit::JscAbi {

enum PropertyAttribute : std::uint32_t {
    ReadOnly = 1u << 1,
    DontEnum = 1u << 2,
    DontDelete = 1u << 3,
    Accessor = 1u << 4
};

enum PropertyDescriptorPresent : std::uint32_t {
    WritablePresent = 1,
    EnumerablePresent = 2,
    ConfigurablePresent = 4
};

constexpr std::uint64_t EncodedJSUndefined = 0x2 | 0x8;

struct PropertyDescriptor {
    std::uint64_t value;
    std::uint64_t getter;
    std::uint64_t setter;
    std::uint32_t attributes;
    std::uint32_t seenAttributes;
};

static_assert(sizeof(PropertyDescriptor) == 0x20);
static_assert(offsetof(PropertyDescriptor, value) == 0x0);
static_assert(offsetof(PropertyDescriptor, getter) == 0x8);
static_assert(offsetof(PropertyDescriptor, setter) == 0x10);
static_assert(offsetof(PropertyDescriptor, attributes) == 0x18);
static_assert(offsetof(PropertyDescriptor, seenAttributes) == 0x1c);

}
