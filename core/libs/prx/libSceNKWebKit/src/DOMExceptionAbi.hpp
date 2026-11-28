#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct DOMExceptionDescription {
    const char* const name;
    const char* const message;
    std::uint8_t legacyCode;
};

static_assert(sizeof(DOMExceptionDescription) == 24);
static_assert(alignof(DOMExceptionDescription) == 8);
static_assert(offsetof(DOMExceptionDescription, name) == 0);
static_assert(offsetof(DOMExceptionDescription, message) == 8);
static_assert(offsetof(DOMExceptionDescription, legacyCode) == 16);
static_assert(std::is_trivially_destructible_v<DOMExceptionDescription>);

}
