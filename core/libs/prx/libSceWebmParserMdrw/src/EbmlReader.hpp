#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace Webm {

struct Element {
    std::uint32_t Id;
    std::size_t DataOffset;
    std::optional<std::uint64_t> DataSize;
};

class EbmlReader {
public:
    static Element ReadElement(std::span<const std::uint8_t> data, std::size_t offset = 0,
        unsigned maxIdLength = 4, unsigned maxSizeLength = 8);
    static std::span<const std::uint8_t> ReadPayload(std::span<const std::uint8_t> data, const Element& element);
    static std::uint64_t ReadUnsigned(std::span<const std::uint8_t> data);
    static std::int64_t ReadSigned(std::span<const std::uint8_t> data);
    static double ReadFloat(std::span<const std::uint8_t> data);
};

}
