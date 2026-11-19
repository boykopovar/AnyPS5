#include "prx/libSceWebmParserMdrw/src/EbmlReader.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

using Webm::EbmlReader;
using Bytes = std::vector<std::uint8_t>;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "WebM EBML: %s\n", message);
    std::abort();
}

template<typename TAction>
void Reject(TAction action, const char* message) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, message);
}

Bytes IntegerBytes(std::uint64_t value, unsigned width) {
    Bytes result(width);
    for (unsigned index = 0; index < width; ++index)
        result[width - index - 1] = static_cast<std::uint8_t>(value >> (index * 8));
    return result;
}

Bytes VintBytes(std::uint64_t value, unsigned width) {
    auto result = IntegerBytes(value, width);
    result.front() |= static_cast<std::uint8_t>(0x80 >> (width - 1));
    return result;
}

void CheckIdentifiers() {
    constexpr std::array<std::uint32_t, 8> ids{
        0x80, 0xfe, 0x407f, 0x7ffe, 0x203fff, 0x3ffffe, 0x101fffff, 0x1ffffffe
    };
    for (unsigned index = 0; index < ids.size(); ++index) {
        auto data = IntegerBytes(ids[index], index / 2 + 1);
        data.push_back(0x80);
        const auto element = EbmlReader::ReadElement(data);
        Require(element.Id == ids[index], "ID marker or value changed");
        Require(element.DataOffset == data.size(), "wrong payload offset");
        Require(element.DataSize == 0, "empty size was not zero");
        Require(EbmlReader::ReadPayload(data, element).empty(), "empty payload was not empty");
    }
    const std::array<Bytes, 11> invalid{
        Bytes{}, Bytes{0}, Bytes{0xff, 0x80}, Bytes{0x40, 0, 0x80},
        Bytes{0x40, 0x7e, 0x80}, Bytes{0x7f, 0xff, 0x80},
        Bytes{0x20, 0x3f, 0xfe, 0x80}, Bytes{0x3f, 0xff, 0xff, 0x80},
        Bytes{0x10, 0x1f, 0xff, 0xfe, 0x80}, Bytes{0x1f, 0xff, 0xff, 0xff, 0x80},
        Bytes{0x08, 0xff, 0xff, 0xff, 0xff, 0x80}
    };
    for (const auto& data : invalid)
        Reject([&] { EbmlReader::ReadElement(data); }, "invalid ID was accepted");
    const Bytes header{0x1a, 0x45, 0xdf, 0xa3, 0x80};
    for (std::size_t length = 0; length < header.size(); ++length)
        Reject([&] { EbmlReader::ReadElement(std::span(header).first(length)); }, "truncated ID or size was accepted");
    Reject([&] { EbmlReader::ReadElement(header, header.size()); }, "EOF offset was accepted");
    Reject([&] { EbmlReader::ReadElement(header, std::numeric_limits<std::size_t>::max()); }, "overflow offset was accepted");
    Reject([&] { EbmlReader::ReadElement(header, 0, 3); }, "document ID limit was ignored");
}

void CheckSizes() {
    for (unsigned width = 1; width <= 8; ++width) {
        for (std::uint64_t size : {0u, 1u, 7u}) {
            Bytes data{0xec};
            const auto encoded = VintBytes(size, width);
            data.insert(data.end(), encoded.begin(), encoded.end());
            data.resize(data.size() + static_cast<std::size_t>(size), 0xa5);
            const auto element = EbmlReader::ReadElement(data);
            Require(element.DataSize == size, "nonminimal size was decoded incorrectly");
            Require(element.DataOffset == width + 1, "wrong size width");
            const auto payload = EbmlReader::ReadPayload(data, element);
            Require(payload.size() == size, "wrong payload size");
            if (size) Require(payload.front() == 0xa5, "payload did not reference the source");
            for (unsigned length = 0; length < width; ++length)
                Reject([&] { EbmlReader::ReadElement(std::span(data).first(1 + length)); }, "truncated size was accepted");
            if (size)
                Reject([&] { EbmlReader::ReadElement(std::span(data).first(data.size() - 1)); }, "truncated payload was accepted");
            if (width > 1)
                Reject([&] { EbmlReader::ReadElement(data, 0, 4, width - 1); }, "document size limit was ignored");
        }
        const auto maximum = (std::uint64_t{1} << (width * 7)) - 1;
        Bytes unknown{0x18, 0x53, 0x80, 0x67};
        const auto encoded = VintBytes(maximum, width);
        unknown.insert(unknown.end(), encoded.begin(), encoded.end());
        const auto element = EbmlReader::ReadElement(unknown);
        Require(!element.DataSize, "unknown size was treated as a finite length");
        Require(element.Id == 0x18538067 && element.DataOffset == unknown.size(), "unknown-size header changed");
        Reject([&] { EbmlReader::ReadPayload(unknown, element); }, "unknown payload consumed the parent implicitly");
        Bytes huge{0xec};
        const auto known = VintBytes(maximum - 1, width);
        huge.insert(huge.end(), known.begin(), known.end());
        Reject([&] { EbmlReader::ReadElement(huge); }, "oversized finite payload was accepted");
    }
    const Bytes zeroPrefix{0xec, 0};
    Reject([&] { EbmlReader::ReadElement(zeroPrefix); }, "zero size prefix was accepted");
    for (auto limits : {std::array<unsigned, 2>{0, 8}, {5, 8}, {4, 0}, {4, 9}}) {
        bool rejected = false;
        try {
            EbmlReader::ReadElement({}, 0, limits[0], limits[1]);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Require(rejected, "unsupported VINT limits were accepted");
    }
    const Bytes empty;
    Reject([&] { EbmlReader::ReadPayload(empty, {0xec, std::numeric_limits<std::size_t>::max(), 1}); }, "payload offset overflow was accepted");
    Reject([&] { EbmlReader::ReadPayload(empty, {0xec, 0, std::numeric_limits<std::uint64_t>::max()}); }, "payload length overflow was accepted");
}

void CheckNestedElements() {
    const Bytes header{
        0x1a, 0x45, 0xdf, 0xa3, 0x8e,
        0x42, 0x82, 0x84, 'w', 'e', 'b', 'm',
        0x42, 0x86, 0x81, 1,
        0x42, 0xf2, 0x80,
        0xec, 0x80
    };
    const auto outer = EbmlReader::ReadElement(header);
    const auto children = EbmlReader::ReadPayload(header, outer);
    auto child = EbmlReader::ReadElement(children);
    Require(child.Id == 0x4282 && child.DataSize == 4, "WebM DocType header was not decoded");
    const auto docType = EbmlReader::ReadPayload(children, child);
    Require(docType[0] == 'w' && docType[3] == 'm', "DocType bytes changed");
    child = EbmlReader::ReadElement(children, child.DataOffset + static_cast<std::size_t>(*child.DataSize));
    Require(child.Id == 0x4286 && EbmlReader::ReadUnsigned(EbmlReader::ReadPayload(children, child)) == 1,
        "EBML version was not decoded");
    child = EbmlReader::ReadElement(children, child.DataOffset + static_cast<std::size_t>(*child.DataSize));
    Require(child.Id == 0x42f2 && child.DataSize == 0, "empty child at parent boundary was lost");
    Require(child.DataOffset == children.size(), "child escaped its parent");
    const auto sibling = EbmlReader::ReadElement(header, outer.DataOffset + static_cast<std::size_t>(*outer.DataSize));
    Require(sibling.Id == 0xec, "following sibling was consumed");
    const Bytes crossesParent{0xa0, 0x82, 0xec, 0x81, 0};
    const auto parent = EbmlReader::ReadElement(crossesParent);
    Reject([&] { EbmlReader::ReadElement(EbmlReader::ReadPayload(crossesParent, parent)); }, "child consumed bytes outside its parent");
}

void CheckNumbers() {
    Require(EbmlReader::ReadUnsigned({}) == 0 && EbmlReader::ReadSigned({}) == 0, "empty integer was not zero");
    Require(EbmlReader::ReadFloat({}) == 0, "empty float was not zero");
    for (unsigned width = 1; width <= 8; ++width) {
        const auto allOnes = IntegerBytes(std::numeric_limits<std::uint64_t>::max(), width);
        const auto maximum = width == 8 ? std::numeric_limits<std::uint64_t>::max() : (std::uint64_t{1} << (width * 8)) - 1;
        Require(EbmlReader::ReadUnsigned(allOnes) == maximum, "unsigned integer was not big endian");
        Require(EbmlReader::ReadSigned(allOnes) == -1, "negative integer was not sign extended");
        const auto sign = std::uint64_t{1} << (width * 8 - 1);
        Require(EbmlReader::ReadSigned(IntegerBytes(sign - 1, width)) == static_cast<std::int64_t>(sign - 1), "positive integer changed");
        const auto minimum = width == 8 ? std::numeric_limits<std::int64_t>::min() : -static_cast<std::int64_t>(sign);
        Require(EbmlReader::ReadSigned(IntegerBytes(sign, width)) == minimum, "signed minimum changed");
    }
    Require(EbmlReader::ReadUnsigned(Bytes{0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef}) == 0x0123456789abcdef,
        "integer byte order changed");
    const Bytes tooWide(9, 0);
    Reject([&] { EbmlReader::ReadUnsigned(tooWide); }, "wide unsigned integer was accepted");
    Reject([&] { EbmlReader::ReadSigned(tooWide); }, "wide signed integer was accepted");
    for (float value : {0.0f, -0.0f, 1.5f, -2.25f, std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::infinity()}) {
        const auto decoded = EbmlReader::ReadFloat(IntegerBytes(std::bit_cast<std::uint32_t>(value), 4));
        Require(decoded == value && std::signbit(decoded) == std::signbit(value), "binary32 value changed");
    }
    for (double value : {0.0, -0.0, 1.5, -2.25, std::numeric_limits<double>::denorm_min(), -std::numeric_limits<double>::infinity()}) {
        const auto bits = std::bit_cast<std::uint64_t>(value);
        Require(std::bit_cast<std::uint64_t>(EbmlReader::ReadFloat(IntegerBytes(bits, 8))) == bits, "binary64 value changed");
    }
    Require(std::isnan(EbmlReader::ReadFloat(Bytes{0x7f, 0xc0, 0, 0})), "binary32 NaN changed");
    Require(std::isnan(EbmlReader::ReadFloat(Bytes{0x7f, 0xf8, 0, 0, 0, 0, 0, 0})), "binary64 NaN changed");
    for (unsigned width : {1u, 2u, 3u, 5u, 6u, 7u, 9u})
        Reject([&] { EbmlReader::ReadFloat(Bytes(width, 0)); }, "invalid float width was accepted");
}

void CheckMalformedCorpus() {
    std::uint32_t state = 0x12345678;
    for (unsigned trial = 0; trial < 4096; ++trial) {
        Bytes data(trial % 65);
        for (auto& byte : data) {
            state = state * 1664525 + 1013904223;
            byte = static_cast<std::uint8_t>(state >> 24);
        }
        for (std::size_t offset = 0; offset <= data.size(); ++offset) {
            try {
                const auto element = EbmlReader::ReadElement(data, offset);
                Require(element.DataOffset > offset && element.DataOffset <= data.size(), "header escaped the input");
                if (element.DataSize) {
                    const auto payload = EbmlReader::ReadPayload(data, element);
                    Require(payload.size() <= data.size() - element.DataOffset, "payload escaped the input");
                }
            } catch (const std::runtime_error&) {
            }
        }
    }
}

}

int main() {
    CheckIdentifiers();
    CheckSizes();
    CheckNestedElements();
    CheckNumbers();
    CheckMalformedCorpus();
    std::puts("WebM EBML tests passed");
}
