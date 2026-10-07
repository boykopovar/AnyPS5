#include <pkg/KrakenStream.hpp>
#include <pkg/PackageError.hpp>

namespace Pkg {

namespace {

constexpr std::uint32_t Half = 0x20000;
constexpr std::size_t MaximumQuantum = 0x3ffff;

void appendBig24(std::vector<std::uint8_t>& stream, const std::uint32_t value) {
    stream.push_back(static_cast<std::uint8_t>(value >> 16));
    stream.push_back(static_cast<std::uint8_t>(value >> 8));
    stream.push_back(static_cast<std::uint8_t>(value));
}

void appendHalf(std::vector<std::uint8_t>& body, const std::uint8_t* payload, const std::size_t payloadSize, const std::uint32_t length, const std::uint32_t flags) {
    if (payloadSize == length) {
        appendBig24(body, 0x800000 | length);
    } else if ((flags & 2) != 0) {
        const std::uint32_t literalMode = (flags & 1) != 0 ? 0 : 1;
        appendBig24(body, 0x800000 | literalMode << 19 | static_cast<std::uint32_t>(payloadSize));
    }
    body.insert(body.end(), payload, payload + payloadSize);
}

}

std::vector<std::uint8_t> BuildKrakenStream(const std::uint8_t* payload, const std::size_t payloadSize, const std::uint32_t length, const std::uint32_t evenPayloadSize, const std::uint32_t flags) {
    if (length == 0 || length > 2 * Half || payloadSize == 0 || payloadSize >= length) throw PackageError("Invalid Kraken block geometry");
    std::vector<std::uint8_t> body;
    body.reserve(payloadSize + 6);
    if (length <= Half) {
        appendHalf(body, payload, payloadSize, length, flags & 0xf);
    } else {
        if (evenPayloadSize == 0 || evenPayloadSize >= payloadSize) throw PackageError("Invalid Kraken block geometry");
        appendHalf(body, payload, evenPayloadSize, Half, flags & 0xf);
        appendHalf(body, payload + evenPayloadSize, payloadSize - evenPayloadSize, length - Half, flags >> 4 & 0xf);
    }
    if (body.size() > MaximumQuantum) throw PackageError("Kraken block is too large for a single quantum");
    std::vector<std::uint8_t> stream{0x8c, 0x06};
    appendBig24(stream, static_cast<std::uint32_t>(body.size() - 1));
    stream.insert(stream.end(), body.begin(), body.end());
    return stream;
}

}
