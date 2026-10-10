#include <io/BufferUtils.hpp>
#include <io/ByteReader.hpp>
#include <io/ByteWriter.hpp>
#include <Testing/Test.hpp>

#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using namespace Testing;

using Bytes = std::vector<std::uint8_t>;

static_assert(Io::AlignUp(std::uint64_t{17}, std::uint64_t{16}) == 32);

constexpr std::uint8_t Filler = 0xCC;
constexpr std::uint64_t Pattern = 0x8877665544332211ull;
constexpr auto MaxOffset = std::numeric_limits<std::size_t>::max();

template<typename TValue>
std::string TypeName() {
    return std::string(std::is_signed_v<TValue> ? "int" : "uint") + std::to_string(sizeof(TValue) * 8);
}

template<typename TCheck>
void ForEachAlignmentType(const TCheck& check) {
    check.template operator()<std::uint32_t>();
    check.template operator()<std::uint64_t>();
    check.template operator()<std::int32_t>();
    check.template operator()<std::int64_t>();
}

template<typename TCheck>
void ForEachByteReaderRead(const TCheck& check) {
    const Io::ByteReader reader;
    check.template operator()<std::uint16_t>([&](const Bytes& bytes, std::size_t offset) { return reader.ReadU16(bytes, offset); }, "ByteReader::ReadU16");
    check.template operator()<std::uint32_t>([&](const Bytes& bytes, std::size_t offset) { return reader.ReadU32(bytes, offset); }, "ByteReader::ReadU32");
    check.template operator()<std::uint64_t>([&](const Bytes& bytes, std::size_t offset) { return reader.ReadU64(bytes, offset); }, "ByteReader::ReadU64");
}

template<typename TCheck>
void ForEachFreeRead(const TCheck& check) {
    check.template operator()<std::uint16_t>(Io::ReadU16, "ReadU16");
    check.template operator()<std::uint32_t>(Io::ReadU32, "ReadU32");
    check.template operator()<std::uint64_t>(Io::ReadU64, "ReadU64");
}

template<typename TCheck>
void ForEachByteWriterWrite(const TCheck& check) {
    const Io::ByteWriter writer;
    check.template operator()<std::uint8_t>([&](Bytes& bytes, std::size_t offset, std::uint8_t value) { writer.WriteU8(bytes, offset, value); }, "ByteWriter::WriteU8");
    check.template operator()<std::uint16_t>([&](Bytes& bytes, std::size_t offset, std::uint16_t value) { writer.WriteU16(bytes, offset, value); }, "ByteWriter::WriteU16");
    check.template operator()<std::uint32_t>([&](Bytes& bytes, std::size_t offset, std::uint32_t value) { writer.WriteU32(bytes, offset, value); }, "ByteWriter::WriteU32");
    check.template operator()<std::uint64_t>([&](Bytes& bytes, std::size_t offset, std::uint64_t value) { writer.WriteU64(bytes, offset, value); }, "ByteWriter::WriteU64");
}

template<typename TCheck>
void ForEachFreeWrite(const TCheck& check) {
    check.template operator()<std::uint8_t>(Io::WriteU8, "WriteU8");
    check.template operator()<std::uint16_t>(Io::WriteU16, "WriteU16");
    check.template operator()<std::uint32_t>(Io::WriteU32, "WriteU32");
    check.template operator()<std::uint64_t>(Io::WriteU64, "WriteU64");
}

template<typename TValue>
Bytes BufferWithValueAtOne(std::size_t trailing) {
    const auto value = static_cast<TValue>(Pattern);
    Bytes bytes(sizeof(value) + 1 + trailing, Filler);
    std::memcpy(bytes.data() + 1, &value, sizeof(value));
    return bytes;
}

template<typename TOperation>
void RequireOutOfRange(const TOperation& operation, const std::string& name, std::size_t offset) {
    RequireThrowsWithMessage<std::out_of_range>(operation, name + " out of bounds", name + " at offset " + std::to_string(offset));
}

template<typename TValue, typename TOperation>
void RequireInvalidOffsetsRejected(const TOperation& operation, const std::string& name) {
    Bytes bytes(sizeof(TValue) + 1, Filler);
    const auto original = bytes;
    for (const std::size_t offset : {bytes.size() - sizeof(TValue) + 1, bytes.size(), bytes.size() + 1,
                                     MaxOffset - sizeof(TValue), MaxOffset - sizeof(TValue) + 1, MaxOffset}) {
        RequireOutOfRange([&] { operation(bytes, offset); }, name, offset);
        Require(bytes == original, name + " modified the buffer on failure");
    }
}

template<typename TValue, typename TOperation>
void RequireShortBuffersRejected(const TOperation& operation, const std::string& name) {
    for (std::size_t size = 0; size < sizeof(TValue); ++size) {
        for (const std::size_t offset : {std::size_t{0}, MaxOffset}) {
            Bytes shortBuffer(size, Filler);
            RequireOutOfRange([&] { operation(shortBuffer, offset); }, name, offset);
            Require(shortBuffer == Bytes(size, Filler), name + " modified a short buffer on failure");
        }
    }
}

const auto UnalignedRead = []<typename TValue, typename TRead>(const TRead& read, const std::string& name) {
    const auto bytes = BufferWithValueAtOne<TValue>(1);

    const TValue value = read(bytes, 1);

    RequireEqual(value, static_cast<TValue>(Pattern), name + " unaligned read");
};

const auto BoundaryRead = []<typename TValue, typename TRead>(const TRead& read, const std::string& name) {
    const auto bytes = BufferWithValueAtOne<TValue>(0);

    const TValue value = read(bytes, 1);

    RequireEqual(value, static_cast<TValue>(Pattern), name + " read ending at the buffer boundary");
};

const auto InvalidReadOffsets = []<typename TValue, typename TRead>(const TRead& read, const std::string& name) {
    RequireInvalidOffsetsRejected<TValue>([&](Bytes& bytes, std::size_t offset) { read(bytes, offset); }, name);
};

const auto ShortReadBuffers = []<typename TValue, typename TRead>(const TRead& read, const std::string& name) {
    RequireShortBuffersRejected<TValue>([&](Bytes& bytes, std::size_t offset) { read(bytes, offset); }, name);
};

const auto UnalignedWrite = []<typename TValue, typename TWrite>(const TWrite& write, const std::string& name) {
    Bytes bytes(sizeof(TValue) + 2, Filler);

    write(bytes, 1, static_cast<TValue>(Pattern));

    Require(bytes == BufferWithValueAtOne<TValue>(1), name + " failed an unaligned write");
};

const auto BoundaryWrite = []<typename TValue, typename TWrite>(const TWrite& write, const std::string& name) {
    Bytes bytes(sizeof(TValue) + 1, Filler);

    write(bytes, 1, static_cast<TValue>(Pattern));

    Require(bytes == BufferWithValueAtOne<TValue>(0), name + " rejected a write ending at the buffer boundary");
};

const auto InvalidWriteOffsets = []<typename TValue, typename TWrite>(const TWrite& write, const std::string& name) {
    RequireInvalidOffsetsRejected<TValue>([&](Bytes& bytes, std::size_t offset) { write(bytes, offset, static_cast<TValue>(Pattern)); }, name);
};

const auto ShortWriteBuffers = []<typename TValue, typename TWrite>(const TWrite& write, const std::string& name) {
    RequireShortBuffersRejected<TValue>([&](Bytes& bytes, std::size_t offset) { write(bytes, offset, static_cast<TValue>(Pattern)); }, name);
};

const Case alignUpZero{"AlignUp_Zero_ReturnsZero", [] {
    ForEachAlignmentType([]<typename TValue>() {
        RequireEqual(Io::AlignUp(TValue{0}, TValue{16}), TValue{0}, "AlignUp(0, 16) for " + TypeName<TValue>());
    });
}};

const Case alignUpRoundsUp{"AlignUp_UnalignedValue_RoundsUpToAlignment", [] {
    ForEachAlignmentType([]<typename TValue>() {
        RequireEqual(Io::AlignUp(TValue{17}, TValue{16}), TValue{32}, "AlignUp(17, 16) for " + TypeName<TValue>());
    });
}};

const Case alignUpNonPowerOfTwo{"AlignUp_NonPowerOfTwoAlignment_RoundsUpToMultiple", [] {
    ForEachAlignmentType([]<typename TValue>() {
        RequireEqual(Io::AlignUp(TValue{7}, TValue{3}), TValue{9}, "AlignUp(7, 3) for " + TypeName<TValue>());
    });
}};

const Case alignUpMaximumByOne{"AlignUp_MaximumWithAlignmentOne_ReturnsMaximum", [] {
    ForEachAlignmentType([]<typename TValue>() {
        constexpr auto maximum = std::numeric_limits<TValue>::max();
        RequireEqual(Io::AlignUp(maximum, TValue{1}), maximum, "AlignUp(max, 1) for " + TypeName<TValue>());
    });
}};

const Case alignUpAlignedNearMaximum{"AlignUp_AlignedValueNearMaximum_ReturnsValueUnchanged", [] {
    ForEachAlignmentType([]<typename TValue>() {
        constexpr auto value = static_cast<TValue>(std::numeric_limits<TValue>::max() - 15);
        RequireEqual(Io::AlignUp(value, TValue{16}), value, "AlignUp(max - 15, 16) for " + TypeName<TValue>());
    });
}};

const Case alignUpRepresentableNearMaximum{"AlignUp_ResultEqualToMaximum_ReturnsMaximum", [] {
    ForEachAlignmentType([]<typename TValue>() {
        constexpr auto maximum = std::numeric_limits<TValue>::max();
        RequireEqual(Io::AlignUp(static_cast<TValue>(maximum - 1), maximum), maximum, "AlignUp(max - 1, max) for " + TypeName<TValue>());
    });
}};

const Case alignUpOverflow{"AlignUp_ResultAboveMaximum_ThrowsOverflowError", [] {
    ForEachAlignmentType([]<typename TValue>() {
        RequireThrows<std::overflow_error>([] { Io::AlignUp(std::numeric_limits<TValue>::max(), TValue{16}); },
                                           "AlignUp(max, 16) for " + TypeName<TValue>());
    });
}};

const Case alignUpZeroAlignment{"AlignUp_ZeroAlignment_ThrowsInvalidArgument", [] {
    ForEachAlignmentType([]<typename TValue>() {
        RequireThrows<std::invalid_argument>([] { Io::AlignUp(TValue{1}, TValue{0}); }, "AlignUp(1, 0) for " + TypeName<TValue>());
    });
}};

const Case alignUpNegativeValue{"AlignUp_NegativeValue_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Io::AlignUp(-1, 16); }, "AlignUp(-1, 16)");
}};

const Case alignUpNegativeAlignment{"AlignUp_NegativeAlignment_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { Io::AlignUp(1, -1); }, "AlignUp(1, -1)");
}};

const Case alignUp64Representable{"AlignUp64_ResultEqualToMaximum_ReturnsMaximum", [] {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();

    const auto aligned = Io::AlignUp64(maximum - 1, maximum);

    RequireEqual(aligned, maximum, "AlignUp64(max - 1, max)");
}};

const Case alignUp64Overflow{"AlignUp64_ResultAboveMaximum_ThrowsOverflowError", [] {
    RequireThrows<std::overflow_error>([] { Io::AlignUp64(std::numeric_limits<std::uint64_t>::max(), 16); }, "AlignUp64(max, 16)");
}};

const Case alignBufferZeroAlignment{"AlignBuffer_ZeroAlignment_ThrowsInvalidArgument", [] {
    Bytes bytes{1, 2, 3};

    RequireThrows<std::invalid_argument>([&] { Io::AlignBuffer(bytes, 0); }, "AlignBuffer(bytes, 0)");
}};

const Case alignBufferZeroAlignmentKeepsBuffer{"AlignBuffer_ZeroAlignment_LeavesBufferUnchanged", [] {
    Bytes bytes{1, 2, 3};

    RequireThrows<std::invalid_argument>([&] { Io::AlignBuffer(bytes, 0); }, "AlignBuffer(bytes, 0)");

    Require(bytes == Bytes({1, 2, 3}), "AlignBuffer modified the buffer on failure");
}};

const Case alignBufferPads{"AlignBuffer_UnalignedBuffer_ZeroFillsPadding", [] {
    Bytes bytes{1, 2, 3};

    Io::AlignBuffer(bytes, 4);

    Require(bytes == Bytes({1, 2, 3, 0}), "AlignBuffer failed to preserve bytes and zero-fill padding");
}};

const Case alignBufferAligned{"AlignBuffer_AlignedBuffer_LeavesBufferUnchanged", [] {
    Bytes bytes{1, 2, 3, 0};

    Io::AlignBuffer(bytes, 4);

    Require(bytes == Bytes({1, 2, 3, 0}), "AlignBuffer changed an aligned buffer");
}};

const Case byteReaderUnaligned{"ByteReader_UnalignedOffset_ReadsValue", [] {
    ForEachByteReaderRead(UnalignedRead);
}};

const Case byteReaderBoundary{"ByteReader_ReadEndingAtBufferEnd_ReadsValue", [] {
    ForEachByteReaderRead(BoundaryRead);
}};

const Case byteReaderInvalidOffsets{"ByteReader_OffsetPastEnd_ThrowsOutOfRangeWithoutModifyingBuffer", [] {
    ForEachByteReaderRead(InvalidReadOffsets);
}};

const Case byteReaderShortBuffers{"ByteReader_BufferShorterThanValue_ThrowsOutOfRangeWithoutModifyingBuffer", [] {
    ForEachByteReaderRead(ShortReadBuffers);
}};

const Case freeReadUnaligned{"ReadU_UnalignedOffset_ReadsValue", [] {
    ForEachFreeRead(UnalignedRead);
}};

const Case freeReadBoundary{"ReadU_ReadEndingAtBufferEnd_ReadsValue", [] {
    ForEachFreeRead(BoundaryRead);
}};

const Case freeReadInvalidOffsets{"ReadU_OffsetPastEnd_ThrowsOutOfRangeWithoutModifyingBuffer", [] {
    ForEachFreeRead(InvalidReadOffsets);
}};

const Case freeReadShortBuffers{"ReadU_BufferShorterThanValue_ThrowsOutOfRangeWithoutModifyingBuffer", [] {
    ForEachFreeRead(ShortReadBuffers);
}};

const Case byteWriterUnaligned{"ByteWriter_UnalignedOffset_WritesValue", [] {
    ForEachByteWriterWrite(UnalignedWrite);
}};

const Case byteWriterBoundary{"ByteWriter_WriteEndingAtBufferEnd_WritesValue", [] {
    ForEachByteWriterWrite(BoundaryWrite);
}};

const Case byteWriterInvalidOffsets{"ByteWriter_OffsetPastEnd_ThrowsOutOfRangeWithoutModifyingBuffer", [] {
    ForEachByteWriterWrite(InvalidWriteOffsets);
}};

const Case byteWriterShortBuffers{"ByteWriter_BufferShorterThanValue_ThrowsOutOfRangeWithoutModifyingBuffer", [] {
    ForEachByteWriterWrite(ShortWriteBuffers);
}};

const Case freeWriteUnaligned{"WriteU_UnalignedOffset_WritesValue", [] {
    ForEachFreeWrite(UnalignedWrite);
}};

const Case freeWriteBoundary{"WriteU_WriteEndingAtBufferEnd_WritesValue", [] {
    ForEachFreeWrite(BoundaryWrite);
}};

const Case freeWriteInvalidOffsets{"WriteU_OffsetPastEnd_ThrowsOutOfRangeWithoutModifyingBuffer", [] {
    ForEachFreeWrite(InvalidWriteOffsets);
}};

const Case freeWriteShortBuffers{"WriteU_BufferShorterThanValue_ThrowsOutOfRangeWithoutModifyingBuffer", [] {
    ForEachFreeWrite(ShortWriteBuffers);
}};

} // namespace
