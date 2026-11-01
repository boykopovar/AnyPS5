#include "prx/libSceFreeTypeOt/include/Stream.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

using namespace FreeType;

namespace {

constexpr int InvalidArgument = 0x06;
constexpr int OutOfMemory = 0x40;
constexpr int InvalidStreamOperation = 0x55;

constexpr std::uint8_t FrameStart = 4;
constexpr std::uint8_t FrameBytes = 24;
constexpr std::uint8_t FrameSkip = 25;
constexpr std::uint8_t FrameSigned = 1;

[[noreturn]] void Unsupported(const char* function, const char* what) {
    throw std::invalid_argument(std::string(function) + ": " + what);
}

GuestStreamRec& Stream(GuestStreamRec* stream, const char* function) {
    if (!stream) Unsupported(function, "null stream");
    return *stream;
}

std::uintptr_t Address(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

std::uint8_t* Offset(const void* pointer, std::uint64_t count) {
    return reinterpret_cast<std::uint8_t*>(Address(pointer) + count);
}

std::uint64_t BigEndian(const std::uint8_t* bytes, unsigned count) {
    std::uint64_t value = 0;
    for (unsigned index = 0; index < count; ++index) value = value << 8 | bytes[index];
    return value;
}

std::uint64_t LittleEndian(const std::uint8_t* bytes, unsigned count) {
    std::uint64_t value = 0;
    for (unsigned index = count; index > 0; --index) value = value << 8 | bytes[index - 1];
    return value;
}

std::int64_t SignExtend(std::uint64_t value, unsigned bytes) {
    const unsigned shift = 64 - bytes * 8;
    return static_cast<std::int64_t>(value << shift) >> shift;
}

void CopyFromBase(std::uint8_t* buffer, const GuestStreamRec& stream, std::uint64_t pos, std::uint64_t count, const char* function) {
    if (count == 0) return;
    if (!buffer) Unsupported(function, "null buffer");
    if (!stream.base) Unsupported(function, "null memory stream base");
    std::memmove(buffer, stream.base + pos, count);
}

std::uint8_t* QAlloc(GuestMemoryRec* memory, std::int64_t size, int& error, const char* function) {
    error = 0;
    if (size > 0) {
        if (!memory || !memory->alloc || !memory->free) Unsupported(function, "null memory allocator or release");
        auto* block = static_cast<std::uint8_t*>(memory->alloc(memory, size));
        if (!block) error = OutOfMemory;
        return block;
    }
    if (size < 0) error = InvalidArgument;
    return nullptr;
}

void Free(GuestMemoryRec* memory, void* block, const char* function) {
    if (!block) return;
    if (!memory || !memory->free) Unsupported(function, "null memory release");
    memory->free(memory, block);
}

int Seek(GuestStreamRec* stream, std::uint64_t pos, const char* function) {
    auto& record = Stream(stream, function);
    int error = 0;
    if (record.read) {
        if (record.read(stream, pos, nullptr, 0)) error = InvalidStreamOperation;
    } else if (pos > record.size) {
        error = InvalidStreamOperation;
    }
    if (!error) record.pos = pos;
    return error;
}

int ReadAt(GuestStreamRec* stream, std::uint64_t pos, std::uint8_t* buffer, std::uint64_t count, const char* function) {
    auto& record = Stream(stream, function);
    if (pos >= record.size) return InvalidStreamOperation;
    std::uint64_t readBytes;
    if (record.read) {
        readBytes = record.read(stream, pos, buffer, count);
    } else {
        readBytes = record.size - pos;
        if (readBytes > count) readBytes = count;
        CopyFromBase(buffer, record, pos, readBytes, function);
    }
    record.pos = pos + readBytes;
    return readBytes < count ? InvalidStreamOperation : 0;
}

int EnterFrame(GuestStreamRec* stream, std::uint64_t count, const char* function) {
    auto& record = Stream(stream, function);
    int error = 0;
    if (record.read) {
        auto* memory = record.memory;
        if (count > record.size) return InvalidStreamOperation;
        record.base = QAlloc(memory, static_cast<std::int64_t>(count), error, function);
        if (error) return error;
        const std::uint64_t readBytes = record.read(stream, record.pos, record.base, count);
        if (readBytes < count) {
            Free(memory, record.base, function);
            record.base = nullptr;
            error = InvalidStreamOperation;
        }
        record.cursor = record.base;
        record.limit = Offset(record.cursor, count);
        record.pos += readBytes;
    } else {
        if (record.pos >= record.size || record.size - record.pos < count) return InvalidStreamOperation;
        record.cursor = Offset(record.base, record.pos);
        record.limit = Offset(record.cursor, count);
        record.pos += count;
    }
    return error;
}

void ExitFrame(GuestStreamRec* stream, const char* function) {
    auto& record = Stream(stream, function);
    if (record.read) {
        Free(record.memory, record.base, function);
        record.base = nullptr;
    }
    record.cursor = nullptr;
    record.limit = nullptr;
}

template <unsigned Bytes, bool Little>
std::uint64_t Get(GuestStreamRec* stream, const char* function) {
    auto& record = Stream(stream, function);
    auto* cursor = record.cursor;
    std::uint64_t value = 0;
    if (Address(cursor) + (Bytes - 1) < Address(record.limit)) {
        if (!cursor) Unsupported(function, "null frame cursor");
        value = Little ? LittleEndian(cursor, Bytes) : BigEndian(cursor, Bytes);
        cursor += Bytes;
    }
    record.cursor = cursor;
    return value;
}

template <unsigned Bytes, bool Little>
std::uint64_t Read(GuestStreamRec* stream, int* error, const char* function) {
    auto& record = Stream(stream, function);
    if (!error) Unsupported(function, "null error");
    *error = 0;
    if (record.pos + (Bytes - 1) >= record.size) {
        *error = InvalidStreamOperation;
        return 0;
    }
    std::uint8_t reads[Bytes]{};
    const std::uint8_t* bytes;
    if (record.read) {
        if (record.read(stream, record.pos, reads, Bytes) != Bytes) {
            *error = InvalidStreamOperation;
            return 0;
        }
        bytes = reads;
    } else {
        bytes = Offset(record.base, record.pos);
        if (bytes && !record.base) Unsupported(function, "null memory stream base");
    }
    std::uint64_t value = 0;
    if (bytes) value = Little ? LittleEndian(bytes, Bytes) : BigEndian(bytes, Bytes);
    record.pos += Bytes;
    return value;
}

template <class T>
void Store(std::uint8_t* target, std::uint64_t value) {
    const auto stored = static_cast<T>(value);
    std::memcpy(target, &stored, sizeof(stored));
}

}

extern "C" {

void APS5_VABI FT_Stream_OpenMemory_nid_postfix(GuestStreamRec* stream, const std::uint8_t* base, std::uint64_t size) {
    auto& record = Stream(stream, __func__);
    record.base = const_cast<std::uint8_t*>(base);
    record.size = size;
    record.pos = 0;
    record.cursor = nullptr;
    record.read = nullptr;
    record.close = nullptr;
}

void APS5_VABI FT_Stream_Close_nid_postfix(GuestStreamRec* stream) {
    if (stream && stream->close) stream->close(stream);
}

int APS5_VABI FT_Stream_Seek_nid_postfix(GuestStreamRec* stream, std::uint64_t pos) {
    return Seek(stream, pos, __func__);
}

int APS5_VABI FT_Stream_Skip_nid_postfix(GuestStreamRec* stream, std::int64_t distance) {
    if (distance < 0) return InvalidStreamOperation;
    const auto& record = Stream(stream, __func__);
    return Seek(stream, record.pos + static_cast<std::uint64_t>(distance), __func__);
}

std::int64_t APS5_VABI FT_Stream_Pos_nid_postfix(GuestStreamRec* stream) {
    return static_cast<std::int64_t>(Stream(stream, __func__).pos);
}

int APS5_VABI FT_Stream_Read_nid_postfix(GuestStreamRec* stream, std::uint8_t* buffer, std::uint64_t count) {
    return ReadAt(stream, Stream(stream, __func__).pos, buffer, count, __func__);
}

int APS5_VABI FT_Stream_ReadAt_nid_postfix(GuestStreamRec* stream, std::uint64_t pos, std::uint8_t* buffer, std::uint64_t count) {
    return ReadAt(stream, pos, buffer, count, __func__);
}

std::uint64_t APS5_VABI FT_Stream_TryRead_nid_postfix(GuestStreamRec* stream, std::uint8_t* buffer, std::uint64_t count) {
    auto& record = Stream(stream, __func__);
    if (record.pos >= record.size) return 0;
    std::uint64_t readBytes;
    if (record.read) {
        readBytes = record.read(stream, record.pos, buffer, count);
    } else {
        readBytes = record.size - record.pos;
        if (readBytes > count) readBytes = count;
        CopyFromBase(buffer, record, record.pos, readBytes, __func__);
    }
    record.pos += readBytes;
    return readBytes;
}

int APS5_VABI FT_Stream_EnterFrame_nid_postfix(GuestStreamRec* stream, std::uint64_t count) {
    return EnterFrame(stream, count, __func__);
}

void APS5_VABI FT_Stream_ExitFrame_nid_postfix(GuestStreamRec* stream) {
    ExitFrame(stream, __func__);
}

int APS5_VABI FT_Stream_ExtractFrame_nid_postfix(GuestStreamRec* stream, std::uint64_t count, std::uint8_t** bytes) {
    if (!bytes) Unsupported(__func__, "null frame pointer");
    const int error = EnterFrame(stream, count, __func__);
    if (!error) {
        *bytes = stream->cursor;
        stream->cursor = nullptr;
        stream->limit = nullptr;
    }
    return error;
}

void APS5_VABI FT_Stream_ReleaseFrame_nid_postfix(GuestStreamRec* stream, std::uint8_t** bytes) {
    if (!bytes) Unsupported(__func__, "null frame pointer");
    if (stream && stream->read) Free(stream->memory, *bytes, __func__);
    *bytes = nullptr;
}

std::int8_t APS5_VABI FT_Stream_GetChar_nid_postfix(GuestStreamRec* stream) {
    auto& record = Stream(stream, __func__);
    std::int8_t value = 0;
    if (Address(record.cursor) < Address(record.limit)) {
        if (!record.cursor) Unsupported(__func__, "null frame cursor");
        value = static_cast<std::int8_t>(*record.cursor);
        record.cursor += 1;
    }
    return value;
}

std::int16_t APS5_VABI FT_Stream_GetShort_nid_postfix(GuestStreamRec* stream) {
    return static_cast<std::int16_t>(Get<2, false>(stream, __func__));
}

std::int16_t APS5_VABI FT_Stream_GetShortLE_nid_postfix(GuestStreamRec* stream) {
    return static_cast<std::int16_t>(Get<2, true>(stream, __func__));
}

std::uint16_t APS5_VABI FT_Stream_GetUShort_nid_postfix(GuestStreamRec* stream) {
    return static_cast<std::uint16_t>(Get<2, false>(stream, __func__));
}

std::uint16_t APS5_VABI FT_Stream_GetUShortLE_nid_postfix(GuestStreamRec* stream) {
    return static_cast<std::uint16_t>(Get<2, true>(stream, __func__));
}

std::int64_t APS5_VABI FT_Stream_GetOffset_nid_postfix(GuestStreamRec* stream) {
    return SignExtend(Get<3, false>(stream, __func__), 3);
}

std::uint64_t APS5_VABI FT_Stream_GetUOffset_nid_postfix(GuestStreamRec* stream) {
    return Get<3, false>(stream, __func__);
}

std::int64_t APS5_VABI FT_Stream_GetLong_nid_postfix(GuestStreamRec* stream) {
    return SignExtend(Get<4, false>(stream, __func__), 4);
}

std::int64_t APS5_VABI FT_Stream_GetLongLE_nid_postfix(GuestStreamRec* stream) {
    return SignExtend(Get<4, true>(stream, __func__), 4);
}

std::uint64_t APS5_VABI FT_Stream_GetULong_nid_postfix(GuestStreamRec* stream) {
    return Get<4, false>(stream, __func__);
}

std::uint64_t APS5_VABI FT_Stream_GetULongLE_nid_postfix(GuestStreamRec* stream) {
    return Get<4, true>(stream, __func__);
}

std::int8_t APS5_VABI FT_Stream_ReadChar_nid_postfix(GuestStreamRec* stream, int* error) {
    auto& record = Stream(stream, __func__);
    if (!error) Unsupported(__func__, "null error");
    *error = 0;
    std::uint8_t value = 0;
    if (record.read) {
        if (record.read(stream, record.pos, &value, 1) != 1) {
            *error = InvalidStreamOperation;
            return 0;
        }
    } else {
        if (record.pos >= record.size) {
            *error = InvalidStreamOperation;
            return 0;
        }
        if (!record.base) Unsupported(__func__, "null memory stream base");
        value = record.base[record.pos];
    }
    record.pos += 1;
    return static_cast<std::int8_t>(value);
}

std::int16_t APS5_VABI FT_Stream_ReadShort_nid_postfix(GuestStreamRec* stream, int* error) {
    return static_cast<std::int16_t>(Read<2, false>(stream, error, __func__));
}

std::int16_t APS5_VABI FT_Stream_ReadShortLE_nid_postfix(GuestStreamRec* stream, int* error) {
    return static_cast<std::int16_t>(Read<2, true>(stream, error, __func__));
}

std::uint16_t APS5_VABI FT_Stream_ReadUShort_nid_postfix(GuestStreamRec* stream, int* error) {
    return static_cast<std::uint16_t>(Read<2, false>(stream, error, __func__));
}

std::uint16_t APS5_VABI FT_Stream_ReadUShortLE_nid_postfix(GuestStreamRec* stream, int* error) {
    return static_cast<std::uint16_t>(Read<2, true>(stream, error, __func__));
}

std::int64_t APS5_VABI FT_Stream_ReadOffset_nid_postfix(GuestStreamRec* stream, int* error) {
    return SignExtend(Read<3, false>(stream, error, __func__), 3);
}

std::uint64_t APS5_VABI FT_Stream_ReadUOffset_nid_postfix(GuestStreamRec* stream, int* error) {
    return Read<3, false>(stream, error, __func__);
}

std::int64_t APS5_VABI FT_Stream_ReadLong_nid_postfix(GuestStreamRec* stream, int* error) {
    return SignExtend(Read<4, false>(stream, error, __func__), 4);
}

std::int64_t APS5_VABI FT_Stream_ReadLongLE_nid_postfix(GuestStreamRec* stream, int* error) {
    return SignExtend(Read<4, true>(stream, error, __func__), 4);
}

std::uint64_t APS5_VABI FT_Stream_ReadULong_nid_postfix(GuestStreamRec* stream, int* error) {
    return Read<4, false>(stream, error, __func__);
}

std::uint64_t APS5_VABI FT_Stream_ReadULongLE_nid_postfix(GuestStreamRec* stream, int* error) {
    return Read<4, true>(stream, error, __func__);
}

int APS5_VABI FT_Stream_ReadFields_nid_postfix(GuestStreamRec* stream, const GuestFrameField* fields, void* structure) {
    const char* function = __func__;
    if (!fields) return InvalidArgument;
    auto& record = Stream(stream, function);
    int error = 0;
    bool frameAccessed = false;
    auto* cursor = record.cursor;
    const auto finish = [&] {
        if (frameAccessed) ExitFrame(stream, function);
        return error;
    };
    const auto refuse = [&](const char* what) {
        if (frameAccessed) ExitFrame(stream, function);
        Unsupported(function, what);
    };
    for (;; ++fields) {
        const GuestFrameField field = *fields;
        if (field.value == FrameStart) {
            error = EnterFrame(stream, field.offset, function);
            if (error) return finish();
            frameAccessed = true;
            cursor = record.cursor;
            continue;
        }
        if (field.value == FrameBytes || field.value == FrameSkip) {
            const std::uint64_t length = field.size;
            if (Address(cursor) + length > Address(record.limit)) {
                error = InvalidStreamOperation;
                return finish();
            }
            if (field.value == FrameBytes && length != 0) {
                if (!structure) refuse("null structure");
                if (!cursor) refuse("null frame cursor");
                std::memmove(static_cast<std::uint8_t*>(structure) + field.offset, cursor, length);
            }
            cursor = Offset(cursor, length);
            continue;
        }
        unsigned bytes = 0;
        bool little = false;
        switch (field.value) {
        case 8: case 9: bytes = 1; break;
        case 12: case 13: bytes = 2; break;
        case 14: case 15: bytes = 2; little = true; break;
        case 16: case 17: bytes = 4; break;
        case 18: case 19: bytes = 4; little = true; break;
        case 20: case 21: bytes = 3; break;
        case 22: case 23: bytes = 3; little = true; break;
        default:
            record.cursor = cursor;
            return finish();
        }
        if (!cursor) refuse("null frame cursor");
        std::uint64_t value = little ? LittleEndian(cursor, bytes) : BigEndian(cursor, bytes);
        cursor += bytes;
        if (field.value & FrameSigned) {
            const unsigned shift = 32 - bytes * 8;
            value = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(value << shift)) >> shift));
        }
        if (!structure) refuse("null structure");
        auto* target = static_cast<std::uint8_t*>(structure) + field.offset;
        switch (field.size) {
        case 1: Store<std::uint8_t>(target, value); break;
        case 2: Store<std::uint16_t>(target, value); break;
        case 4: Store<std::uint32_t>(target, value); break;
        default: Store<std::uint64_t>(target, value); break;
        }
    }
}

void APS5_VABI FT_Stream_Free_nid_postfix(GuestStreamRec* stream, std::int32_t external) {
    if (!stream) return;
    auto* memory = stream->memory;
    if (!external && (!memory || !memory->free)) Unsupported(__func__, "null memory release");
    FT_Stream_Close_nid_postfix(stream);
    if (!external) Free(memory, stream, __func__);
}

}
