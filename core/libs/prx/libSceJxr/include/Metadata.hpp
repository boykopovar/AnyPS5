#pragma once

#include <cstddef>
#include <cstdint>

namespace Jxr {

struct MetadataVariant {
    std::int32_t type;
    union {
        std::uint8_t byte;
        std::uint16_t shortValue;
        std::uint32_t longValue;
        char* text;
        std::uint16_t* wideText;
        std::uint8_t* bytes;
    } value;
};

static_assert(sizeof(MetadataVariant) == 16 && alignof(MetadataVariant) == 8);
static_assert(offsetof(MetadataVariant, type) == 0 && offsetof(MetadataVariant, value) == 8);

} // namespace Jxr
