#include "prx/libSceWebmParserMdrw/src/EbmlReader.hpp"

#include <bit>
#include <limits>
#include <stdexcept>
#include <string>

namespace Webm {

namespace {

struct Vint {
    std::uint64_t Value;
    unsigned Width;
};

[[noreturn]] void Fail(const char* reason, std::size_t offset) {
    throw std::runtime_error(std::string("WebM EBML: ") + reason + " at byte " + std::to_string(offset));
}

Vint ReadVint(std::span<const std::uint8_t> data, std::size_t offset, unsigned maximum) {
    if (offset >= data.size()) Fail("truncated variable integer", offset);
    unsigned width = 1;
    unsigned marker = 0x80;
    while (marker && !(data[offset] & marker)) {
        marker >>= 1;
        ++width;
    }
    if (!marker || width > maximum) Fail("invalid variable integer width", offset);
    if (width > data.size() - offset) Fail("truncated variable integer", offset);
    std::uint64_t value = data[offset] & (marker - 1);
    for (unsigned index = 1; index < width; ++index) value = (value << 8) | data[offset + index];
    return {value, width};
}

std::uint64_t MaximumVint(unsigned width) {
    return (std::uint64_t{1} << (7 * width)) - 1;
}

}

Element EbmlReader::ReadElement(std::span<const std::uint8_t> data, std::size_t offset,
    unsigned maxIdLength, unsigned maxSizeLength) {
    if (maxIdLength == 0 || maxIdLength > 4 || maxSizeLength == 0 || maxSizeLength > 8)
        throw std::invalid_argument("WebM EBML: unsupported maximum variable integer length");
    const auto id = ReadVint(data, offset, maxIdLength);
    if (id.Value == MaximumVint(id.Width) ||
        (id.Width > 1 && id.Value < MaximumVint(id.Width - 1)))
        Fail("reserved or noncanonical element ID", offset);
    const auto sizeOffset = offset + id.Width;
    const auto size = ReadVint(data, sizeOffset, maxSizeLength);
    Element result{
        static_cast<std::uint32_t>(id.Value | (std::uint64_t{1} << (7 * id.Width))),
        sizeOffset + size.Width,
        std::nullopt
    };
    if (size.Value != MaximumVint(size.Width)) {
        result.DataSize = size.Value;
        ReadPayload(data, result);
    }
    return result;
}

std::span<const std::uint8_t> EbmlReader::ReadPayload(std::span<const std::uint8_t> data, const Element& element) {
    if (!element.DataSize) Fail("unknown element size requires schema resolution", element.DataOffset);
    if (element.DataOffset > data.size() || *element.DataSize > data.size() - element.DataOffset)
        Fail("element payload exceeds its parent", element.DataOffset);
    return data.subspan(element.DataOffset, static_cast<std::size_t>(*element.DataSize));
}

std::uint64_t EbmlReader::ReadUnsigned(std::span<const std::uint8_t> data) {
    if (data.size() > 8) throw std::runtime_error("WebM EBML: integer exceeds eight bytes");
    std::uint64_t result = 0;
    for (auto byte : data) result = (result << 8) | byte;
    return result;
}

std::int64_t EbmlReader::ReadSigned(std::span<const std::uint8_t> data) {
    auto result = ReadUnsigned(data);
    if (!data.empty() && data.size() < 8 && (data.front() & 0x80))
        result |= std::numeric_limits<std::uint64_t>::max() << (data.size() * 8);
    return std::bit_cast<std::int64_t>(result);
}

double EbmlReader::ReadFloat(std::span<const std::uint8_t> data) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
    if (data.empty()) return 0;
    if (data.size() == 4) return std::bit_cast<float>(static_cast<std::uint32_t>(ReadUnsigned(data)));
    if (data.size() == 8) return std::bit_cast<double>(ReadUnsigned(data));
    throw std::runtime_error("WebM EBML: float requires zero, four or eight bytes");
}

}
