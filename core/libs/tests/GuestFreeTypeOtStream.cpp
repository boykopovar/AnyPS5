#include "prx/libSceFreeTypeOt/include/Stream.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

namespace {

using FreeType::GuestMemoryRec;
using FreeType::GuestStreamRec;

constexpr int InvalidStreamOperation = 0x55;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot stream: %s\n", message);
    std::exit(1);
}

struct Recorder {
    GuestMemoryRec memory{};
    int allocs = 0;
    int frees = 0;
    std::int64_t lastSize = 0;
    void* lastBlock = nullptr;
};

Recorder* Owner(GuestMemoryRec* memory) {
    return static_cast<Recorder*>(memory->user);
}

void* APS5_VABI RecordAlloc(GuestMemoryRec* memory, std::int64_t size) {
    auto* recorder = Owner(memory);
    ++recorder->allocs;
    recorder->lastSize = size;
    recorder->lastBlock = std::malloc(static_cast<std::size_t>(size));
    return recorder->lastBlock;
}

void APS5_VABI RecordFree(GuestMemoryRec* memory, void* block) {
    auto* recorder = Owner(memory);
    ++recorder->frees;
    recorder->lastBlock = block;
    std::free(block);
}

void* APS5_VABI RecordRealloc(GuestMemoryRec* memory, std::int64_t, std::int64_t size, void* block) {
    auto* recorder = Owner(memory);
    recorder->lastBlock = std::realloc(block, static_cast<std::size_t>(size));
    return recorder->lastBlock;
}

struct Source {
    std::vector<std::uint8_t> data;
    bool shortReads = false;
    bool seekFails = false;
};

std::uint64_t APS5_VABI ReadCallback(GuestStreamRec* stream, std::uint64_t pos, std::uint8_t* buffer, std::uint64_t count) {
    auto* source = static_cast<Source*>(stream->descriptor);
    if (count == 0) return source->seekFails ? 1 : 0;
    if (source->shortReads) return count - 1;
    if (pos >= source->data.size()) return 0;
    const std::uint64_t available = source->data.size() - pos;
    const std::uint64_t readBytes = std::min(available, count);
    std::memcpy(buffer, source->data.data() + pos, static_cast<std::size_t>(readBytes));
    return readBytes;
}

struct MemoryFixture {
    std::vector<std::uint8_t> data;
    GuestStreamRec stream{};

    explicit MemoryFixture(std::vector<std::uint8_t> bytes) : data(std::move(bytes)) {
        std::memset(&stream, 0, sizeof(stream));
        FT_Stream_OpenMemory_nid_postfix(&stream, data.data(), data.size());
    }
};

struct CallbackFixture {
    Source source;
    Recorder recorder;
    GuestStreamRec stream{};

    explicit CallbackFixture(std::vector<std::uint8_t> bytes) {
        source.data = std::move(bytes);
        recorder.memory = {&recorder, RecordAlloc, RecordFree, RecordRealloc};
        std::memset(&stream, 0, sizeof(stream));
        stream.size = source.data.size();
        stream.descriptor = &source;
        stream.read = ReadCallback;
        stream.memory = &recorder.memory;
    }
};

template <class ReadFn>
void ExpectRead(GuestStreamRec* stream, std::uint64_t start, ReadFn read, std::int64_t expected, std::uint64_t width, const char* message) {
    Require(FT_Stream_Seek_nid_postfix(stream, start) == 0, message);
    int error = -1;
    const auto value = static_cast<std::int64_t>(read(stream, &error));
    Require(error == 0 && value == expected && FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(start + width), message);
}

template <class ReadFn>
void ExpectReadFailure(GuestStreamRec* stream, std::uint64_t start, ReadFn read, const char* message) {
    Require(FT_Stream_Seek_nid_postfix(stream, start) == 0, message);
    int error = -1;
    const auto value = static_cast<std::int64_t>(read(stream, &error));
    Require(error == InvalidStreamOperation && value == 0 && FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(start), message);
}

template <class GetFn>
void ExpectGet(GuestStreamRec* stream, std::uint64_t start, std::uint64_t count, GetFn get, std::int64_t expected, std::uint64_t width, const char* message) {
    Require(FT_Stream_Seek_nid_postfix(stream, start) == 0, message);
    Require(FT_Stream_EnterFrame_nid_postfix(stream, count) == 0, message);
    std::uint8_t* const before = stream->cursor;
    const auto value = static_cast<std::int64_t>(get(stream));
    Require(value == expected && stream->cursor == before + width, message);
    FT_Stream_ExitFrame_nid_postfix(stream);
}

template <class GetFn>
void ExpectGetPastLimit(GuestStreamRec* stream, std::uint64_t start, std::uint64_t count, GetFn get, const char* message) {
    Require(FT_Stream_Seek_nid_postfix(stream, start) == 0, message);
    Require(FT_Stream_EnterFrame_nid_postfix(stream, count) == 0, message);
    std::uint8_t* const before = stream->cursor;
    const auto value = static_cast<std::int64_t>(get(stream));
    Require(value == 0 && stream->cursor == before, message);
    FT_Stream_ExitFrame_nid_postfix(stream);
}

template <class ReadFn>
void CheckReadBoth(const std::vector<std::uint8_t>& data, std::uint64_t start, ReadFn read, std::int64_t expected, std::uint64_t width, const char* message) {
    MemoryFixture memory(data);
    ExpectRead(&memory.stream, start, read, expected, width, message);
    CallbackFixture callback(data);
    ExpectRead(&callback.stream, start, read, expected, width, message);
}

template <class ReadFn>
void CheckReadFailureBoth(const std::vector<std::uint8_t>& data, std::uint64_t start, ReadFn read, const char* message) {
    MemoryFixture memory(data);
    ExpectReadFailure(&memory.stream, start, read, message);
    CallbackFixture callback(data);
    ExpectReadFailure(&callback.stream, start, read, message);
}

template <class ReadFn>
void CheckShortReadBoth(const std::vector<std::uint8_t>& data, ReadFn read, const char* message) {
    CallbackFixture callback(data);
    callback.source.shortReads = true;
    ExpectReadFailure(&callback.stream, 0, read, message);
}

template <class GetFn>
void CheckGetBoth(const std::vector<std::uint8_t>& data, std::uint64_t start, GetFn get, std::int64_t expected, std::uint64_t width, const char* message) {
    MemoryFixture memory(data);
    ExpectGet(&memory.stream, start, width, get, expected, width, message);
    CallbackFixture callback(data);
    ExpectGet(&callback.stream, start, width, get, expected, width, message);
}

template <class GetFn>
void CheckGetPastLimitBoth(const std::vector<std::uint8_t>& data, GetFn get, std::uint64_t width, const char* message) {
    const std::uint64_t count = width - 1;
    MemoryFixture memory(data);
    ExpectGetPastLimit(&memory.stream, 0, count, get, message);
    CallbackFixture callback(data);
    ExpectGetPastLimit(&callback.stream, 0, count, get, message);
}

void ReadCharTests() {
    const std::vector<std::uint8_t> data = {0x7F, 0x80, 0xFF, 0x00, 0x01};

    {
        MemoryFixture fixture(data);
        auto* stream = &fixture.stream;
        Require(stream->base == fixture.data.data() && stream->size == fixture.data.size() && stream->pos == 0 && stream->cursor == nullptr &&
                    stream->limit == nullptr && stream->read == nullptr && stream->close == nullptr,
                "a memory stream is opened with no callback");
        int error = -1;
        Require(FT_Stream_ReadChar_nid_postfix(stream, &error) == 0x7F && error == 0 && FT_Stream_Pos_nid_postfix(stream) == 1,
                "ReadChar reads a positive signed byte");
        error = -1;
        Require(FT_Stream_ReadChar_nid_postfix(stream, &error) == -128 && error == 0 && FT_Stream_Pos_nid_postfix(stream) == 2,
                "ReadChar reads the most negative signed byte");
        error = -1;
        Require(FT_Stream_ReadChar_nid_postfix(stream, &error) == -1 && error == 0 && FT_Stream_Pos_nid_postfix(stream) == 3,
                "ReadChar reads a negative signed byte");
        Require(FT_Stream_Seek_nid_postfix(stream, data.size()) == 0, "a memory stream seeks to the end");
        error = -1;
        Require(FT_Stream_ReadChar_nid_postfix(stream, &error) == 0 && error == InvalidStreamOperation &&
                    FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(data.size()),
                "ReadChar at the memory end fails without advancing");
    }

    {
        CallbackFixture fixture(data);
        auto* stream = &fixture.stream;
        Require(stream->read != nullptr && stream->size == data.size() && stream->pos == 0, "a callback stream has a read callback");
        int error = -1;
        Require(FT_Stream_ReadChar_nid_postfix(stream, &error) == 0x7F && error == 0 && FT_Stream_Pos_nid_postfix(stream) == 1,
                "ReadChar reads a positive signed byte from a callback");
        error = -1;
        Require(FT_Stream_ReadChar_nid_postfix(stream, &error) == -128 && error == 0 && FT_Stream_Pos_nid_postfix(stream) == 2,
                "ReadChar reads the most negative signed byte from a callback");
        error = -1;
        Require(FT_Stream_ReadChar_nid_postfix(stream, &error) == -1 && error == 0 && FT_Stream_Pos_nid_postfix(stream) == 3,
                "ReadChar reads a negative signed byte from a callback");

        fixture.source.shortReads = true;
        Require(FT_Stream_Seek_nid_postfix(stream, 0) == 0, "a short callback stream rewinds");
        error = -1;
        Require(FT_Stream_ReadChar_nid_postfix(stream, &error) == 0 && error == InvalidStreamOperation && FT_Stream_Pos_nid_postfix(stream) == 0,
                "ReadChar fails on a short callback read without advancing");

        fixture.source.shortReads = false;
        Require(FT_Stream_Seek_nid_postfix(stream, data.size()) == 0, "a callback stream seeks to the end");
        error = -1;
        Require(FT_Stream_ReadChar_nid_postfix(stream, &error) == 0 && error == InvalidStreamOperation &&
                    FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(data.size()),
                "ReadChar at the callback end fails without advancing");
    }
}

void ReadShortTests() {
    const std::vector<std::uint8_t> data = {0x80, 0x01, 0x01, 0x80, 0x7F, 0xFF, 0xFF, 0x7F};
    CheckReadBoth(data, 0, FT_Stream_ReadShort_nid_postfix, -32767, 2, "ReadShort decodes a negative big-endian value");
    CheckReadBoth(data, 0, FT_Stream_ReadUShort_nid_postfix, 32769, 2, "ReadUShort decodes an unsigned high-bit big-endian value");
    CheckReadBoth(data, 2, FT_Stream_ReadShortLE_nid_postfix, -32767, 2, "ReadShortLE decodes a negative little-endian value");
    CheckReadBoth(data, 2, FT_Stream_ReadUShortLE_nid_postfix, 32769, 2, "ReadUShortLE decodes an unsigned high-bit little-endian value");
    CheckReadBoth(data, 4, FT_Stream_ReadShort_nid_postfix, 32767, 2, "ReadShort decodes a positive big-endian value");
    CheckReadBoth(data, 6, FT_Stream_ReadShortLE_nid_postfix, 32767, 2, "ReadShortLE decodes a positive little-endian value");
    CheckReadFailureBoth(data, data.size() - 1, FT_Stream_ReadShort_nid_postfix, "ReadShort fails on a one-byte tail");
    CheckReadFailureBoth(data, data.size() - 1, FT_Stream_ReadUShort_nid_postfix, "ReadUShort fails on a one-byte tail");
    CheckReadFailureBoth(data, data.size() - 1, FT_Stream_ReadShortLE_nid_postfix, "ReadShortLE fails on a one-byte tail");
    CheckReadFailureBoth(data, data.size() - 1, FT_Stream_ReadUShortLE_nid_postfix, "ReadUShortLE fails on a one-byte tail");
    CheckReadFailureBoth(data, data.size(), FT_Stream_ReadShort_nid_postfix, "ReadShort fails at the end");
    CheckShortReadBoth(data, FT_Stream_ReadShort_nid_postfix, "ReadShort fails on a short callback read");
    CheckShortReadBoth(data, FT_Stream_ReadUShort_nid_postfix, "ReadUShort fails on a short callback read");
    CheckShortReadBoth(data, FT_Stream_ReadShortLE_nid_postfix, "ReadShortLE fails on a short callback read");
    CheckShortReadBoth(data, FT_Stream_ReadUShortLE_nid_postfix, "ReadUShortLE fails on a short callback read");
}

void ReadOffsetTests() {
    const std::vector<std::uint8_t> data = {0x80, 0x00, 0x01, 0x00, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF};
    CheckReadBoth(data, 0, FT_Stream_ReadOffset_nid_postfix, -8388607, 3, "ReadOffset decodes a negative three-byte value");
    CheckReadBoth(data, 0, FT_Stream_ReadUOffset_nid_postfix, 8388609, 3, "ReadUOffset decodes an unsigned high-bit three-byte value");
    CheckReadBoth(data, 3, FT_Stream_ReadOffset_nid_postfix, 2, 3, "ReadOffset decodes a positive three-byte value");
    CheckReadBoth(data, 3, FT_Stream_ReadUOffset_nid_postfix, 2, 3, "ReadUOffset decodes a small three-byte value");
    CheckReadBoth(data, 6, FT_Stream_ReadOffset_nid_postfix, -1, 3, "ReadOffset sign-extends all ones");
    CheckReadBoth(data, 6, FT_Stream_ReadUOffset_nid_postfix, 16777215, 3, "ReadUOffset keeps all ones unsigned");
    CheckReadBoth(data, 9, FT_Stream_ReadOffset_nid_postfix, 8388607, 3, "ReadOffset decodes the largest positive three-byte value");
    CheckReadBoth(data, 9, FT_Stream_ReadUOffset_nid_postfix, 8388607, 3, "ReadUOffset decodes the largest three-byte value");
    CheckReadFailureBoth(data, data.size() - 2, FT_Stream_ReadOffset_nid_postfix, "ReadOffset fails on a two-byte tail");
    CheckReadFailureBoth(data, data.size() - 2, FT_Stream_ReadUOffset_nid_postfix, "ReadUOffset fails on a two-byte tail");
    CheckReadFailureBoth(data, data.size() - 1, FT_Stream_ReadOffset_nid_postfix, "ReadOffset fails on a one-byte tail");
    CheckReadFailureBoth(data, data.size(), FT_Stream_ReadUOffset_nid_postfix, "ReadUOffset fails at the end");
    CheckShortReadBoth(data, FT_Stream_ReadOffset_nid_postfix, "ReadOffset fails on a short callback read");
    CheckShortReadBoth(data, FT_Stream_ReadUOffset_nid_postfix, "ReadUOffset fails on a short callback read");
}

void ReadLongTests() {
    const std::vector<std::uint8_t> data = {0x80, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x80, 0x7F, 0xFF,
                                            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF};
    CheckReadBoth(data, 0, FT_Stream_ReadLong_nid_postfix, -2147483647, 4, "ReadLong decodes a negative big-endian value");
    CheckReadBoth(data, 0, FT_Stream_ReadULong_nid_postfix, 2147483649, 4, "ReadULong decodes an unsigned high-bit big-endian value");
    CheckReadBoth(data, 4, FT_Stream_ReadLongLE_nid_postfix, -2147483647, 4, "ReadLongLE decodes a negative little-endian value");
    CheckReadBoth(data, 4, FT_Stream_ReadULongLE_nid_postfix, 2147483649, 4, "ReadULongLE decodes an unsigned high-bit little-endian value");
    CheckReadBoth(data, 8, FT_Stream_ReadLong_nid_postfix, 2147483647, 4, "ReadLong decodes a positive big-endian value");
    CheckReadBoth(data, 12, FT_Stream_ReadLongLE_nid_postfix, 2147483647, 4, "ReadLongLE decodes a positive little-endian value");
    CheckReadBoth(data, 16, FT_Stream_ReadLong_nid_postfix, -1, 4, "ReadLong sign-extends all ones");
    CheckReadBoth(data, 16, FT_Stream_ReadLongLE_nid_postfix, -1, 4, "ReadLongLE sign-extends all ones");
    CheckReadBoth(data, 16, FT_Stream_ReadULong_nid_postfix, 4294967295, 4, "ReadULong keeps all ones unsigned");
    CheckReadBoth(data, 16, FT_Stream_ReadULongLE_nid_postfix, 4294967295, 4, "ReadULongLE keeps all ones unsigned");
    CheckReadFailureBoth(data, data.size() - 3, FT_Stream_ReadLong_nid_postfix, "ReadLong fails on a three-byte tail");
    CheckReadFailureBoth(data, data.size() - 1, FT_Stream_ReadLongLE_nid_postfix, "ReadLongLE fails on a one-byte tail");
    CheckReadFailureBoth(data, data.size(), FT_Stream_ReadULong_nid_postfix, "ReadULong fails at the end");
    CheckShortReadBoth(data, FT_Stream_ReadLong_nid_postfix, "ReadLong fails on a short callback read");
    CheckShortReadBoth(data, FT_Stream_ReadLongLE_nid_postfix, "ReadLongLE fails on a short callback read");
    CheckShortReadBoth(data, FT_Stream_ReadULong_nid_postfix, "ReadULong fails on a short callback read");
    CheckShortReadBoth(data, FT_Stream_ReadULongLE_nid_postfix, "ReadULongLE fails on a short callback read");
}

void SkipTests() {
    const std::vector<std::uint8_t> data = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0};

    {
        MemoryFixture fixture(data);
        auto* stream = &fixture.stream;
        Require(FT_Stream_Skip_nid_postfix(stream, 4) == 0 && FT_Stream_Pos_nid_postfix(stream) == 4, "Skip moves a memory stream forward");
        Require(FT_Stream_Skip_nid_postfix(stream, 6) == 0 && FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(data.size()),
                "Skip reaches the exact memory end");
        Require(FT_Stream_Skip_nid_postfix(stream, 1) == InvalidStreamOperation && FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(data.size()),
                "Skip past the memory end fails without advancing");
        Require(FT_Stream_Seek_nid_postfix(stream, 3) == 0, "a memory stream rewinds");
        Require(FT_Stream_Skip_nid_postfix(stream, -1) == InvalidStreamOperation && FT_Stream_Pos_nid_postfix(stream) == 3,
                "Skip rejects a negative distance without advancing");
    }

    {
        CallbackFixture fixture(data);
        auto* stream = &fixture.stream;
        Require(FT_Stream_Skip_nid_postfix(stream, 4) == 0 && FT_Stream_Pos_nid_postfix(stream) == 4, "Skip moves a callback stream forward");
        Require(FT_Stream_Skip_nid_postfix(stream, 6) == 0 && FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(data.size()),
                "Skip reaches the exact callback end");
        Require(FT_Stream_Skip_nid_postfix(stream, -1) == InvalidStreamOperation && FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(data.size()),
                "Skip rejects a negative callback distance without advancing");

        fixture.source.seekFails = true;
        Require(FT_Stream_Skip_nid_postfix(stream, 1) == InvalidStreamOperation && FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(data.size()),
                "Skip fails when the callback rejects the seek without advancing");
    }
}

void FrameTests() {
    const std::vector<std::uint8_t> data = {0xFE, 0x80, 0x01, 0x01, 0x80, 0x80, 0x00, 0x01, 0xFF, 0xFF,
                                            0xFF, 0x80, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x80, 0xFF,
                                            0xFF, 0xFF, 0xFF};

    CheckGetBoth(data, 0, FT_Stream_GetChar_nid_postfix, -2, 1, "GetChar reads a signed byte inside a frame");
    CheckGetBoth(data, 1, FT_Stream_GetShort_nid_postfix, -32767, 2, "GetShort reads a negative big-endian value inside a frame");
    CheckGetBoth(data, 1, FT_Stream_GetUShort_nid_postfix, 32769, 2, "GetUShort reads an unsigned high-bit value inside a frame");
    CheckGetBoth(data, 3, FT_Stream_GetShortLE_nid_postfix, -32767, 2, "GetShortLE reads a negative little-endian value inside a frame");
    CheckGetBoth(data, 3, FT_Stream_GetUShortLE_nid_postfix, 32769, 2, "GetUShortLE reads an unsigned high-bit value inside a frame");
    CheckGetBoth(data, 5, FT_Stream_GetOffset_nid_postfix, -8388607, 3, "GetOffset reads a negative three-byte value inside a frame");
    CheckGetBoth(data, 5, FT_Stream_GetUOffset_nid_postfix, 8388609, 3, "GetUOffset reads an unsigned high-bit value inside a frame");
    CheckGetBoth(data, 8, FT_Stream_GetOffset_nid_postfix, -1, 3, "GetOffset sign-extends all ones inside a frame");
    CheckGetBoth(data, 8, FT_Stream_GetUOffset_nid_postfix, 16777215, 3, "GetUOffset keeps all ones unsigned inside a frame");
    CheckGetBoth(data, 11, FT_Stream_GetLong_nid_postfix, -2147483647, 4, "GetLong reads a negative big-endian value inside a frame");
    CheckGetBoth(data, 11, FT_Stream_GetULong_nid_postfix, 2147483649, 4, "GetULong reads an unsigned high-bit value inside a frame");
    CheckGetBoth(data, 15, FT_Stream_GetLongLE_nid_postfix, -2147483647, 4, "GetLongLE reads a negative little-endian value inside a frame");
    CheckGetBoth(data, 15, FT_Stream_GetULongLE_nid_postfix, 2147483649, 4, "GetULongLE reads an unsigned high-bit value inside a frame");
    CheckGetBoth(data, 19, FT_Stream_GetLong_nid_postfix, -1, 4, "GetLong sign-extends all ones inside a frame");
    CheckGetBoth(data, 19, FT_Stream_GetLongLE_nid_postfix, -1, 4, "GetLongLE sign-extends all ones inside a frame");
    CheckGetBoth(data, 19, FT_Stream_GetULong_nid_postfix, 4294967295, 4, "GetULong keeps all ones unsigned inside a frame");
    CheckGetBoth(data, 19, FT_Stream_GetULongLE_nid_postfix, 4294967295, 4, "GetULongLE keeps all ones unsigned inside a frame");

    CheckGetPastLimitBoth(data, FT_Stream_GetChar_nid_postfix, 1, "GetChar past the frame limit returns zero without advancing");
    CheckGetPastLimitBoth(data, FT_Stream_GetShort_nid_postfix, 2, "GetShort past the frame limit returns zero without advancing");
    CheckGetPastLimitBoth(data, FT_Stream_GetShortLE_nid_postfix, 2, "GetShortLE past the frame limit returns zero without advancing");
    CheckGetPastLimitBoth(data, FT_Stream_GetUShort_nid_postfix, 2, "GetUShort past the frame limit returns zero without advancing");
    CheckGetPastLimitBoth(data, FT_Stream_GetUShortLE_nid_postfix, 2, "GetUShortLE past the frame limit returns zero without advancing");
    CheckGetPastLimitBoth(data, FT_Stream_GetOffset_nid_postfix, 3, "GetOffset past the frame limit returns zero without advancing");
    CheckGetPastLimitBoth(data, FT_Stream_GetUOffset_nid_postfix, 3, "GetUOffset past the frame limit returns zero without advancing");
    CheckGetPastLimitBoth(data, FT_Stream_GetLong_nid_postfix, 4, "GetLong past the frame limit returns zero without advancing");
    CheckGetPastLimitBoth(data, FT_Stream_GetLongLE_nid_postfix, 4, "GetLongLE past the frame limit returns zero without advancing");
    CheckGetPastLimitBoth(data, FT_Stream_GetULong_nid_postfix, 4, "GetULong past the frame limit returns zero without advancing");
    CheckGetPastLimitBoth(data, FT_Stream_GetULongLE_nid_postfix, 4, "GetULongLE past the frame limit returns zero without advancing");

    {
        MemoryFixture fixture(data);
        auto* stream = &fixture.stream;
        Require(FT_Stream_Seek_nid_postfix(stream, data.size()) == 0, "a memory stream seeks to the end for a frame");
        Require(FT_Stream_EnterFrame_nid_postfix(stream, 1) == InvalidStreamOperation && stream->cursor == nullptr && stream->limit == nullptr &&
                    FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(data.size()),
                "a frame at the memory end fails without changing the frame or position");
        Require(FT_Stream_Seek_nid_postfix(stream, data.size() - 2) == 0, "a memory stream seeks into the tail");
        Require(FT_Stream_EnterFrame_nid_postfix(stream, 4) == InvalidStreamOperation && stream->cursor == nullptr && stream->limit == nullptr &&
                    FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(data.size() - 2),
                "a frame longer than the memory tail fails without changing the frame or position");
        Require(FT_Stream_Seek_nid_postfix(stream, 0) == 0, "a memory stream rewinds for a frame");
        Require(FT_Stream_EnterFrame_nid_postfix(stream, 4) == 0, "a memory frame is entered");
        Require(stream->base == fixture.data.data() && stream->cursor == fixture.data.data() && stream->limit == fixture.data.data() + 4 &&
                    FT_Stream_Pos_nid_postfix(stream) == 4,
                "a memory frame points into the base and advances the position");
        Require(FT_Stream_GetChar_nid_postfix(stream) == -2 && stream->cursor == fixture.data.data() + 1, "a memory frame advances its cursor");
        Require(FT_Stream_GetLong_nid_postfix(stream) == 0 && stream->cursor == fixture.data.data() + 1,
                "a memory frame read past the limit leaves the cursor unchanged");
        FT_Stream_ExitFrame_nid_postfix(stream);
        Require(stream->cursor == nullptr && stream->limit == nullptr && stream->base == fixture.data.data(),
                "exiting a memory frame clears the cursor and limit but keeps the base");
    }

    {
        CallbackFixture fixture(data);
        auto* stream = &fixture.stream;
        Require(FT_Stream_EnterFrame_nid_postfix(stream, data.size() + 1) == InvalidStreamOperation && fixture.recorder.allocs == 0 &&
                    fixture.recorder.frees == 0 && stream->base == nullptr && stream->cursor == nullptr && stream->limit == nullptr &&
                    FT_Stream_Pos_nid_postfix(stream) == 0,
                "a callback frame longer than the stream fails without allocating");

        Require(FT_Stream_Seek_nid_postfix(stream, data.size() - 2) == 0, "a callback stream seeks into the tail");
        Require(FT_Stream_EnterFrame_nid_postfix(stream, 4) == InvalidStreamOperation && fixture.recorder.allocs == 1 &&
                    fixture.recorder.frees == 1 && stream->base == nullptr && stream->cursor == nullptr && stream->limit == nullptr &&
                    FT_Stream_Pos_nid_postfix(stream) == static_cast<std::int64_t>(data.size()),
                "a callback frame over the tail frees the frame and clears the cursor and limit");

        fixture.source.shortReads = true;
        Require(FT_Stream_Seek_nid_postfix(stream, 0) == 0, "a short callback stream rewinds for a frame");
        Require(FT_Stream_EnterFrame_nid_postfix(stream, 4) == InvalidStreamOperation && fixture.recorder.allocs == 2 &&
                    fixture.recorder.frees == 2 && stream->base == nullptr && stream->cursor == nullptr && stream->limit == nullptr &&
                    FT_Stream_Pos_nid_postfix(stream) == 3,
                "a short callback frame frees the frame and clears the cursor and limit");
        fixture.source.shortReads = false;

        Require(FT_Stream_Seek_nid_postfix(stream, 0) == 0, "a callback stream rewinds for a frame");
        Require(FT_Stream_EnterFrame_nid_postfix(stream, 4) == 0 && fixture.recorder.allocs == 3 && fixture.recorder.frees == 2 &&
                    stream->base != nullptr && stream->cursor == stream->base && stream->limit == stream->base + 4 &&
                    FT_Stream_Pos_nid_postfix(stream) == 4,
                "a callback frame allocates a block and advances the position");
        Require(FT_Stream_GetChar_nid_postfix(stream) == -2 && stream->cursor == stream->base + 1, "a callback frame advances its cursor");
        Require(FT_Stream_GetLong_nid_postfix(stream) == 0 && stream->cursor == stream->base + 1,
                "a callback frame read past the limit leaves the cursor unchanged");
        FT_Stream_ExitFrame_nid_postfix(stream);
        Require(fixture.recorder.allocs == 3 && fixture.recorder.frees == 3 && stream->base == nullptr && stream->cursor == nullptr &&
                    stream->limit == nullptr,
                "exiting a callback frame frees the block and clears the cursor and limit");
    }
}

}

int main() {
    ReadCharTests();
    ReadShortTests();
    ReadOffsetTests();
    ReadLongTests();
    SkipTests();
    FrameTests();
    std::puts("Guest FreeType Ot stream tests passed");
    return 0;
}
