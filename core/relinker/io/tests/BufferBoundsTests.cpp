#include <io/BufferUtils.hpp>
#include <io/ByteReader.hpp>
#include <io/ByteWriter.hpp>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using Bytes = std::vector<std::uint8_t>;

static_assert(Io::AlignUp(std::uint64_t{17}, std::uint64_t{16}) == 32);

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TOperation>
void requireOutOfRange(const TOperation& operation, const std::string& name, std::size_t offset) {
    try {
        operation();
    } catch (const std::out_of_range& error) {
        require(std::string(error.what()) == name + " out of bounds", name + " changed its out-of-range diagnostic");
        return;
    }
    throw std::runtime_error(name + " accepted invalid offset " + std::to_string(offset));
}

template<typename TValue, typename TOperation>
void invalidRanges(Bytes& bytes, const TOperation& operation, const std::string& name) {
    const auto original = bytes;
    constexpr auto maxOffset = std::numeric_limits<std::size_t>::max();
    for (const std::size_t offset : {bytes.size() - sizeof(TValue) + 1, bytes.size(), bytes.size() + 1,
            maxOffset - sizeof(TValue), maxOffset - sizeof(TValue) + 1, maxOffset}) {
        requireOutOfRange([&] { operation(bytes, offset); }, name, offset);
        require(bytes == original, name + " modified the buffer on failure");
    }
    for (std::size_t size = 0; size < sizeof(TValue); ++size) {
        Bytes shortBuffer(size, 0xCC);
        requireOutOfRange([&] { operation(shortBuffer, 0); }, name, 0);
        require(shortBuffer == Bytes(size, 0xCC), name + " modified a short buffer on failure");
        requireOutOfRange([&] { operation(shortBuffer, maxOffset); }, name, maxOffset);
        require(shortBuffer == Bytes(size, 0xCC), name + " modified a short buffer on failure");
    }
}

template<typename TValue, typename TRead>
void readBounds(const TRead& read, const std::string& name) {
    const auto value = static_cast<TValue>(0x8877665544332211ull);
    Bytes bytes(sizeof(value) + 2, 0xCC);
    std::memcpy(bytes.data() + 1, &value, sizeof(value));
    require(read(bytes, 1) == value, name + " failed an unaligned read");
    bytes.pop_back();
    require(read(bytes, 1) == value, name + " rejected a read ending at the buffer boundary");
    invalidRanges<TValue>(bytes, read, name);
}

template<typename TValue, typename TWrite>
void writeBounds(const TWrite& write, const std::string& name) {
    const auto value = static_cast<TValue>(0x8877665544332211ull);
    Bytes bytes(sizeof(value) + 2, 0xCC);
    auto expected = bytes;
    std::memcpy(expected.data() + 1, &value, sizeof(value));
    write(bytes, 1, value);
    require(bytes == expected, name + " failed an unaligned write");
    bytes.pop_back();
    expected.pop_back();
    bytes.assign(expected.size(), 0xCC);
    write(bytes, 1, value);
    require(bytes == expected, name + " rejected a write ending at the buffer boundary");
    invalidRanges<TValue>(bytes, [&](Bytes& buffer, std::size_t offset) { write(buffer, offset, value); }, name);
}

template<typename TError, typename TOperation>
void requireThrows(const TOperation& operation, const std::string& message) {
    try {
        operation();
    } catch (const TError&) {
        return;
    }
    throw std::runtime_error(message);
}

template<typename TValue>
void alignmentBounds() {
    constexpr auto maximum = std::numeric_limits<TValue>::max();
    require(Io::AlignUp(TValue{0}, TValue{16}) == 0, "AlignUp changed zero");
    require(Io::AlignUp(TValue{17}, TValue{16}) == 32, "AlignUp failed to round up");
    require(Io::AlignUp(TValue{7}, TValue{3}) == 9, "AlignUp requires a power-of-two alignment");
    require(Io::AlignUp(maximum, TValue{1}) == maximum, "AlignUp changed an aligned maximum");
    require(Io::AlignUp(static_cast<TValue>(maximum - 15), TValue{16}) == maximum - 15, "AlignUp changed an aligned value near the maximum");
    require(Io::AlignUp(static_cast<TValue>(maximum - 1), maximum) == maximum, "AlignUp overflowed for a representable result");
    requireThrows<std::overflow_error>([&] { Io::AlignUp(maximum, TValue{16}); }, "AlignUp accepted an overflowing result");
    requireThrows<std::invalid_argument>([] { Io::AlignUp(TValue{1}, TValue{0}); }, "AlignUp accepted zero alignment");
}

}

int main() {
    try {
        const Io::ByteReader reader;
        const Io::ByteWriter writer;
        alignmentBounds<std::uint32_t>();
        alignmentBounds<std::uint64_t>();
        alignmentBounds<std::int32_t>();
        alignmentBounds<std::int64_t>();
        requireThrows<std::invalid_argument>([] { Io::AlignUp(-1, 16); }, "AlignUp accepted a negative value");
        requireThrows<std::invalid_argument>([] { Io::AlignUp(1, -1); }, "AlignUp accepted negative alignment");
        require(Io::AlignUp64(std::numeric_limits<std::uint64_t>::max() - 1, std::numeric_limits<std::uint64_t>::max()) == std::numeric_limits<std::uint64_t>::max(), "AlignUp64 overflowed for a representable result");
        requireThrows<std::overflow_error>([] { Io::AlignUp64(std::numeric_limits<std::uint64_t>::max(), 16); }, "AlignUp64 accepted an overflowing result");
        Bytes aligned{1, 2, 3};
        const auto original = aligned;
        requireThrows<std::invalid_argument>([&] { Io::AlignBuffer(aligned, 0); }, "AlignBuffer accepted zero alignment");
        require(aligned == original, "AlignBuffer modified the buffer on failure");
        Io::AlignBuffer(aligned, 4);
        require(aligned == Bytes({1, 2, 3, 0}), "AlignBuffer failed to preserve bytes and zero-fill padding");
        Io::AlignBuffer(aligned, 4);
        require(aligned == Bytes({1, 2, 3, 0}), "AlignBuffer changed an aligned buffer");
        readBounds<std::uint16_t>([&](const Bytes& bytes, std::size_t offset) { return reader.ReadU16(bytes, offset); }, "ByteReader::ReadU16");
        readBounds<std::uint32_t>([&](const Bytes& bytes, std::size_t offset) { return reader.ReadU32(bytes, offset); }, "ByteReader::ReadU32");
        readBounds<std::uint64_t>([&](const Bytes& bytes, std::size_t offset) { return reader.ReadU64(bytes, offset); }, "ByteReader::ReadU64");
        writeBounds<std::uint8_t>([&](Bytes& bytes, std::size_t offset, std::uint8_t value) { writer.WriteU8(bytes, offset, value); }, "ByteWriter::WriteU8");
        writeBounds<std::uint16_t>([&](Bytes& bytes, std::size_t offset, std::uint16_t value) { writer.WriteU16(bytes, offset, value); }, "ByteWriter::WriteU16");
        writeBounds<std::uint32_t>([&](Bytes& bytes, std::size_t offset, std::uint32_t value) { writer.WriteU32(bytes, offset, value); }, "ByteWriter::WriteU32");
        writeBounds<std::uint64_t>([&](Bytes& bytes, std::size_t offset, std::uint64_t value) { writer.WriteU64(bytes, offset, value); }, "ByteWriter::WriteU64");
        readBounds<std::uint16_t>(Io::ReadU16, "ReadU16");
        readBounds<std::uint32_t>(Io::ReadU32, "ReadU32");
        readBounds<std::uint64_t>(Io::ReadU64, "ReadU64");
        writeBounds<std::uint8_t>(Io::WriteU8, "WriteU8");
        writeBounds<std::uint16_t>(Io::WriteU16, "WriteU16");
        writeBounds<std::uint32_t>(Io::WriteU32, "WriteU32");
        writeBounds<std::uint64_t>(Io::WriteU64, "WriteU64");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
