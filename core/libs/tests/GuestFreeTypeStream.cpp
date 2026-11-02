#include "prx/libSceFreeTypeOt/include/Stream.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using FreeType::GuestFrameField;
using FreeType::GuestMemoryRec;
using FreeType::GuestStreamRec;

#define STREAM_EXPORTS(X) \
    X(OpenMemory, "a7RlARfexoo") X(Close, "gVf7xRLdWm8") X(Seek, "R1mZutnlTiM") X(Skip, "4J4Baf8ana8") \
    X(Pos, "jX1zq1i8KfI") X(Read, "U+0WKVAePww") X(ReadAt, "dlbnv6Wass8") X(TryRead, "Az4H9FlfxCA") \
    X(EnterFrame, "FHdeCFVf-f8") X(ExitFrame, "Qmgc0XACzRg") X(ExtractFrame, "0rGx-oFMLyk") X(ReleaseFrame, "xejRyEbsLcw") \
    X(GetChar, "jl0VxYZb8-o") X(GetShort, "dXJK59vXRxs") X(GetShortLE, "BB4gl7SgLdk") X(GetUShort, "u6Xquj1bFwc") \
    X(GetUShortLE, "-Xm1cLEpBec") X(GetOffset, "tDssQCH2syY") X(GetUOffset, "4yt9cD1qcEI") X(GetLong, "Er0Lin-u20o") \
    X(GetLongLE, "KHj92a3WDn4") X(GetULong, "Fv8MWCUH8k8") X(GetULongLE, "RzgDJTjQst4") X(ReadChar, "cmd41395g7Y") \
    X(ReadShort, "mF0B64O954g") X(ReadShortLE, "c002A4zTOgA") X(ReadUShort, "6a1p3QgXvo0") X(ReadUShortLE, "JFSCOXTEvfg") \
    X(ReadOffset, "1z6bBg1qBfU") X(ReadUOffset, "S+n0Md8Rjfw") X(ReadLong, "Bmi8PdvsM3M") X(ReadLongLE, "jJh4SMt8NFQ") \
    X(ReadULong, "mcT796Qgj+g") X(ReadULongLE, "b5gZp-r3WWI") X(ReadFields, "SX5I+XzH8HI") X(Free, "thA48N-0VwQ")

namespace {

#define DECLARE(name, nid) decltype(&FT_Stream_##name##_nid_postfix) name = nullptr;
struct Api {
    STREAM_EXPORTS(DECLARE)
};
#undef DECLARE

Api api;

constexpr int InvalidArgument = 0x06;
constexpr int OutOfMemory = 0x40;
constexpr int InvalidStreamOperation = 0x55;
constexpr auto Max = std::numeric_limits<std::uint64_t>::max();

void Require(bool result, const char* message) {
    if (result) return;
    std::fprintf(stderr, "FreeType stream: %s\n", message);
    std::exit(1);
}

template <class Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

std::uint8_t* At(std::uint64_t address) {
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address));
}

struct ReadCall {
    GuestStreamRec* stream;
    std::uint64_t offset;
    std::uint8_t* buffer;
    std::uint64_t count;
};

struct Device {
    std::vector<std::uint8_t> source;
    std::vector<ReadCall> calls;
    std::uint64_t seekResult = 0;
    std::uint64_t shortBy = 0;
    unsigned closes = 0;
    std::vector<std::int64_t> allocations;
    std::vector<void*> releases;
    bool failAllocation = false;
    std::vector<char> order;
};

Device device;

std::uint64_t APS5_VABI DeviceRead(GuestStreamRec* stream, std::uint64_t offset, std::uint8_t* buffer, std::uint64_t count) {
    device.calls.push_back({stream, offset, buffer, count});
    if (!buffer && count == 0) return device.seekResult;
    std::uint64_t available = offset < device.source.size() ? device.source.size() - offset : 0;
    if (available > count) available = count;
    available = available > device.shortBy ? available - device.shortBy : 0;
    if (available) std::memcpy(buffer, device.source.data() + offset, available);
    return available;
}

void APS5_VABI DeviceClose(GuestStreamRec* stream) {
    ++device.closes;
    device.order.push_back('c');
    stream->memory = nullptr;
}

void* APS5_VABI DeviceAlloc(GuestMemoryRec*, std::int64_t size) {
    device.allocations.push_back(size);
    return device.failAllocation ? nullptr : std::malloc(static_cast<std::size_t>(size));
}

void APS5_VABI DeviceRelease(GuestMemoryRec*, void* block) {
    device.releases.push_back(block);
    device.order.push_back('f');
    std::free(block);
}

GuestMemoryRec memory{nullptr, DeviceAlloc, DeviceRelease, nullptr};

GuestStreamRec MemoryStream(std::uint8_t* base, std::uint64_t size) {
    GuestStreamRec stream{};
    api.OpenMemory(&stream, base, size);
    return stream;
}

GuestStreamRec DeviceStream(std::vector<std::uint8_t> source, std::uint64_t size) {
    device = {};
    device.source = std::move(source);
    GuestStreamRec stream{};
    stream.size = size;
    stream.read = DeviceRead;
    stream.memory = &memory;
    return stream;
}

void TestOpenMemoryAndPosition() {
    std::array<std::uint8_t, 8> data{{0x80, 0x01, 0x02, 0x03, 0xFE, 0xFF, 0x7F, 0x10}};
    GuestStreamRec stream;
    std::memset(&stream, 0xCD, sizeof(stream));
    stream.descriptor = At(0x11);
    stream.pathname = At(0x22);
    stream.memory = reinterpret_cast<GuestMemoryRec*>(At(0x33));
    stream.limit = At(0x44);
    api.OpenMemory(&stream, data.data(), data.size());
    Require(stream.base == data.data() && stream.size == 8 && stream.pos == 0, "open memory sets base, size and position");
    Require(!stream.cursor && !stream.read && !stream.close, "open memory clears cursor and callbacks");
    Require(stream.limit == At(0x44) && stream.memory == reinterpret_cast<GuestMemoryRec*>(At(0x33)), "open memory keeps limit and memory");
    Require(stream.descriptor == At(0x11) && stream.pathname == At(0x22), "open memory keeps descriptor and path name");
    Require(api.Pos(&stream) == 0, "initial position");
    Require(api.Seek(&stream, 8) == 0 && stream.pos == 8, "seek to the end is valid");
    Require(api.Seek(&stream, 9) == InvalidStreamOperation && stream.pos == 8, "seek past the end fails and keeps the position");
    Require(api.Skip(nullptr, -1) == InvalidStreamOperation, "negative skip fails before reading the stream");
    stream.pos = 3;
    Require(api.Skip(&stream, -1) == InvalidStreamOperation && stream.pos == 3, "negative skip keeps the position");
    Require(api.Skip(&stream, 5) == 0 && stream.pos == 8, "skip to the end");
    Require(api.Skip(&stream, std::numeric_limits<std::int64_t>::max()) == InvalidStreamOperation && stream.pos == 8, "skip past the end");
    stream.pos = Max - 1;
    Require(api.Skip(&stream, 3) == 0 && stream.pos == 1, "skip wraps as unsigned long");
    stream.pos = Max;
    Require(api.Pos(&stream) == -1, "position is returned as a signed long");
    Require(api.Seek(&stream, 0) == 0 && api.Pos(&stream) == 0, "seek to the start");
}

void TestMemoryReads() {
    std::array<std::uint8_t, 8> data{{0x80, 0x01, 0x02, 0x03, 0xFE, 0xFF, 0x7F, 0x10}};
    auto stream = MemoryStream(data.data(), data.size());
    std::array<std::uint8_t, 8> out{};
    Require(api.ReadAt(&stream, 2, out.data(), 3) == 0 && stream.pos == 5, "read at an offset");
    Require(out[0] == 0x02 && out[1] == 0x03 && out[2] == 0xFE, "read-at bytes");
    out.fill(0);
    Require(api.ReadAt(&stream, 6, out.data(), 4) == InvalidStreamOperation && stream.pos == 8, "short read copies what remains and fails");
    Require(out[0] == 0x7F && out[1] == 0x10 && out[2] == 0, "short read bytes");
    stream.pos = 4;
    Require(api.ReadAt(&stream, 8, out.data(), 1) == InvalidStreamOperation && stream.pos == 4, "read at the end fails without moving");
    Require(api.ReadAt(&stream, 0, nullptr, 0) == 0 && stream.pos == 0, "empty read accepts a null buffer");
    Require(Throws([&] { api.ReadAt(&stream, 0, nullptr, 1); }), "null buffer with bytes to copy is unsupported");
    stream.pos = 1;
    out.fill(0);
    Require(api.Read(&stream, out.data(), 2) == 0 && stream.pos == 3 && out[0] == 0x01 && out[1] == 0x02, "read at the current position");
    stream.pos = 6;
    Require(api.TryRead(&stream, out.data(), 5) == 2 && stream.pos == 8 && out[0] == 0x7F, "try-read returns the bytes available");
    Require(api.TryRead(&stream, out.data(), 5) == 0 && stream.pos == 8, "try-read at the end");
    stream.pos = 1;
    Require(api.EnterFrame(&stream, 4) == 0, "enter a frame");
    Require(stream.cursor == data.data() + 1 && stream.limit == data.data() + 5 && stream.pos == 5, "memory frame points into the stream");
    Require(api.GetUShort(&stream) == 0x0102 && api.GetChar(&stream) == 3 && api.GetChar(&stream) == -2, "frame values");
    Require(api.GetChar(&stream) == 0 && api.GetUShort(&stream) == 0 && stream.cursor == data.data() + 5, "reads at the frame end return 0 and keep the cursor");
    api.ExitFrame(&stream);
    Require(!stream.cursor && !stream.limit && stream.base == data.data(), "exit keeps a memory stream's base");
    stream.pos = 8;
    Require(api.EnterFrame(&stream, 0) == InvalidStreamOperation, "frame at the end fails");
    stream.pos = 6;
    Require(api.EnterFrame(&stream, 3) == InvalidStreamOperation && stream.pos == 6 && !stream.cursor, "frame past the end fails");
    stream.pos = 0;
    Require(api.EnterFrame(&stream, 0) == 0 && stream.cursor == data.data() && stream.limit == data.data() && stream.pos == 0, "empty frame");
    api.ExitFrame(&stream);
    std::uint8_t* frame = nullptr;
    stream.pos = 2;
    Require(api.ExtractFrame(&stream, 3, &frame) == 0 && frame == data.data() + 2 && !stream.cursor && !stream.limit && stream.pos == 5, "extract a memory frame");
    api.ReleaseFrame(&stream, &frame);
    Require(!frame, "release a memory frame");
    frame = At(0x99);
    Require(api.ExtractFrame(&stream, 4, &frame) == InvalidStreamOperation && frame == At(0x99), "failed extraction leaves the pointer");
    api.ReleaseFrame(nullptr, &frame);
    Require(!frame, "release without a stream clears the pointer");
    stream.pos = 1;
    Require(Throws([&] { api.ExtractFrame(&stream, 3, nullptr); }), "null memory frame pointer is unsupported");
    Require(stream.pos == 1 && !stream.cursor && !stream.limit, "null memory frame pointer enters no frame");
}

void TestFrameGetters() {
    std::array<std::uint8_t, 4> data{{0x80, 0x01, 0x02, 0x03}};
    GuestStreamRec stream{};
    const auto frame = [&](unsigned length) {
        stream.cursor = data.data();
        stream.limit = data.data() + length;
    };
    frame(4);
    Require(api.GetShort(&stream) == -32767 && stream.cursor == data.data() + 2, "signed big-endian short");
    frame(4);
    Require(api.GetShortLE(&stream) == 0x0180, "signed little-endian short");
    frame(4);
    Require(api.GetUShort(&stream) == 0x8001, "unsigned big-endian short");
    frame(4);
    Require(api.GetUShortLE(&stream) == 0x0180, "unsigned little-endian short");
    frame(4);
    Require(api.GetOffset(&stream) == 0x800102 - 0x1000000 && stream.cursor == data.data() + 3, "signed 24-bit offset");
    frame(4);
    Require(api.GetUOffset(&stream) == 0x800102, "unsigned 24-bit offset");
    frame(4);
    Require(api.GetLong(&stream) == static_cast<std::int32_t>(0x80010203u), "signed big-endian long");
    frame(4);
    Require(api.GetLongLE(&stream) == 0x03020180, "signed little-endian long");
    frame(4);
    Require(api.GetULong(&stream) == 0x80010203u && stream.cursor == data.data() + 4, "unsigned big-endian long");
    frame(4);
    Require(api.GetULongLE(&stream) == 0x03020180u, "unsigned little-endian long");
    frame(1);
    Require(api.GetUShort(&stream) == 0 && stream.cursor == data.data(), "short past the frame");
    frame(2);
    Require(api.GetUOffset(&stream) == 0 && api.GetULong(&stream) == 0 && stream.cursor == data.data(), "offset and long past the frame");
    Require(api.GetChar(&stream) == -128, "signed char");
    stream.cursor = nullptr;
    stream.limit = nullptr;
    Require(api.GetChar(&stream) == 0 && api.GetULong(&stream) == 0 && !stream.cursor, "no frame");
    stream.limit = At(8);
    Require(Throws([&] { api.GetChar(&stream); }) && Throws([&] { api.GetUShort(&stream); }), "null cursor below the limit is unsupported");
}

void TestMemoryReaders() {
    std::array<std::uint8_t, 4> data{{0x80, 0x01, 0x02, 0x03}};
    auto stream = MemoryStream(data.data(), data.size());
    int error = -1;
    const auto read = [&](auto function, std::uint64_t pos) {
        stream.pos = pos;
        error = -1;
        return function(&stream, &error);
    };
    Require(read(api.ReadShort, 0) == -32767 && error == 0 && stream.pos == 2, "read signed short");
    Require(read(api.ReadShortLE, 0) == 0x0180 && stream.pos == 2, "read signed little-endian short");
    Require(read(api.ReadUShort, 2) == 0x0203 && stream.pos == 4, "read unsigned short");
    Require(read(api.ReadUShortLE, 2) == 0x0302 && error == 0, "read unsigned little-endian short");
    Require(read(api.ReadOffset, 0) == 0x800102 - 0x1000000 && stream.pos == 3, "read signed offset");
    Require(read(api.ReadUOffset, 1) == 0x010203 && stream.pos == 4, "read unsigned offset");
    Require(read(api.ReadLong, 0) == static_cast<std::int32_t>(0x80010203u), "read signed long");
    Require(read(api.ReadLongLE, 0) == 0x03020180, "read signed little-endian long");
    Require(read(api.ReadULong, 0) == 0x80010203u && stream.pos == 4, "read unsigned long");
    Require(read(api.ReadULongLE, 0) == 0x03020180u, "read unsigned little-endian long");
    Require(read(api.ReadChar, 0) == -128 && error == 0 && stream.pos == 1, "read char");
    Require(read(api.ReadChar, 4) == 0 && error == InvalidStreamOperation && stream.pos == 4, "read char at the end");
    Require(read(api.ReadULong, 1) == 0 && error == InvalidStreamOperation && stream.pos == 1, "long past the end");
    Require(read(api.ReadUShort, 3) == 0 && error == InvalidStreamOperation && stream.pos == 3, "short past the end");
    std::array<std::uint8_t, 6> window{{0x11, 0x22, 0x33, 0x44, 0x55, 0x66}};
    stream = MemoryStream(window.data() + 1, 4);
    Require(read(api.ReadUShort, Max) == 0x1122 && error == 0 && stream.pos == 1, "position plus size wraps as unsigned long");
    Require(read(api.ReadChar, Max) == 0 && error == InvalidStreamOperation, "char at the maximum position");
    stream = MemoryStream(nullptr, 4);
    Require(read(api.ReadUShort, 0) == 0 && error == 0 && stream.pos == 2, "null base at position 0 reads 0");
    Require(Throws([&] { read(api.ReadUShort, 2); }), "null base past position 0 is unsupported");
    Require(Throws([&] { read(api.ReadChar, 0); }), "null base char is unsupported");
    Require(Throws([&] { api.ReadULong(&stream, nullptr); }), "null error pointer is unsupported");
}

void TestDeviceStream() {
    auto stream = DeviceStream({1, 2, 3, 4, 5, 6, 7, 8}, 8);
    Require(api.Seek(&stream, 1000) == 0 && stream.pos == 1000, "device seek does not check the size");
    Require(device.calls.size() == 1 && device.calls[0].stream == &stream && device.calls[0].offset == 1000 && !device.calls[0].buffer && device.calls[0].count == 0, "seek calls read with no buffer");
    device.seekResult = 1;
    Require(api.Seek(&stream, 2) == InvalidStreamOperation && stream.pos == 1000, "failed device seek keeps the position");
    device.seekResult = 0;
    std::array<std::uint8_t, 8> out{};
    device.calls.clear();
    Require(api.ReadAt(&stream, 8, out.data(), 1) == InvalidStreamOperation && device.calls.empty(), "read at the end does not call the device");
    Require(api.ReadAt(&stream, 2, out.data(), 3) == 0 && stream.pos == 5 && out[0] == 3 && out[2] == 5, "device read");
    Require(device.calls.size() == 1 && device.calls[0].offset == 2 && device.calls[0].buffer == out.data() && device.calls[0].count == 3, "device read arguments");
    device.shortBy = 1;
    Require(api.ReadAt(&stream, 4, out.data(), 3) == InvalidStreamOperation && stream.pos == 6, "short device read");
    stream.pos = 6;
    Require(api.TryRead(&stream, out.data(), 4) == 1 && stream.pos == 7, "device try-read");
    device.shortBy = 0;
    device.calls.clear();
    stream.pos = 8;
    Require(api.TryRead(&stream, out.data(), 4) == 0 && device.calls.empty(), "try-read at the end does not call the device");
    stream.pos = 2;
    Require(api.EnterFrame(&stream, 4) == 0 && device.allocations.size() == 1 && device.allocations[0] == 4, "device frame allocates its size");
    auto* block = stream.base;
    Require(block && stream.cursor == block && stream.limit == block + 4 && stream.pos == 6, "device frame");
    Require(device.calls.back().offset == 2 && device.calls.back().buffer == block && device.calls.back().count == 4, "device frame read");
    Require(api.GetULong(&stream) == 0x03040506u, "device frame value");
    api.ExitFrame(&stream);
    Require(device.releases.size() == 1 && device.releases[0] == block && !stream.base && !stream.cursor && !stream.limit, "exit releases a device frame");
    device.allocations.clear();
    Require(api.EnterFrame(&stream, 9) == InvalidStreamOperation && device.allocations.empty(), "frame larger than the stream");
    device.failAllocation = true;
    stream.base = At(0x77);
    Require(api.EnterFrame(&stream, 2) == OutOfMemory && !stream.base && stream.pos == 6, "allocation failure");
    device.failAllocation = false;
    device.releases.clear();
    stream.pos = 6;
    Require(api.EnterFrame(&stream, 4) == InvalidStreamOperation, "short frame read fails");
    Require(device.releases.size() == 1 && !stream.base && !stream.cursor && stream.limit == At(4) && stream.pos == 8, "short frame read releases the block");
    stream.memory = nullptr;
    device.calls.clear();
    Require(api.EnterFrame(&stream, 0) == 0 && !stream.base && !stream.cursor && !stream.limit, "empty device frame allocates nothing");
    Require(device.calls.size() == 1 && !device.calls[0].buffer && device.calls[0].count == 0, "empty device frame reads nothing");
    api.ExitFrame(&stream);
    stream.memory = &memory;
    stream.size = Max;
    device.allocations.clear();
    stream.base = At(0x77);
    Require(api.EnterFrame(&stream, std::uint64_t{1} << 63) == InvalidArgument && !stream.base && device.allocations.empty(), "frame size above LONG_MAX");
    stream.size = 8;
    stream.pos = 0;
    std::uint8_t* frame = nullptr;
    Require(api.ExtractFrame(&stream, 2, &frame) == 0 && frame && frame == stream.base && !stream.cursor && !stream.limit, "extract a device frame");
    device.releases.clear();
    api.ReleaseFrame(&stream, &frame);
    Require(!frame && device.releases.size() == 1, "release a device frame");
    Require(Throws([&] { api.ReleaseFrame(&stream, nullptr); }), "null frame pointer is unsupported");
    device.allocations.clear();
    device.calls.clear();
    device.releases.clear();
    stream.base = At(0x55);
    stream.pos = 1;
    Require(Throws([&] { api.ExtractFrame(&stream, 2, nullptr); }), "null device frame pointer is unsupported");
    Require(device.allocations.empty() && device.calls.empty() && device.releases.empty(), "null device frame pointer allocates and reads nothing");
    Require(stream.base == At(0x55) && stream.pos == 1 && !stream.cursor && !stream.limit, "null device frame pointer leaves the stream");
    GuestMemoryRec noRelease{nullptr, DeviceAlloc, nullptr, nullptr};
    stream.memory = &noRelease;
    Require(Throws([&] { api.EnterFrame(&stream, 2); }), "allocator without release is unsupported");
    Require(device.allocations.empty() && device.calls.empty() && stream.base == At(0x55) && stream.pos == 1, "allocator without release allocates and reads nothing");
    Require(api.EnterFrame(&stream, 9) == InvalidStreamOperation && device.calls.empty(), "frame larger than the stream does not need the allocator");
    Require(api.EnterFrame(&stream, 0) == 0 && device.allocations.empty() && !stream.base, "empty frame does not need the allocator");
    api.ExitFrame(&stream);
    stream.memory = &memory;
    int error = -1;
    stream.base = nullptr;
    stream.pos = 0;
    device.calls.clear();
    Require(api.ReadULong(&stream, &error) == 0x01020304u && error == 0 && stream.pos == 4, "device long");
    Require(device.calls.size() == 1 && device.calls[0].offset == 0 && device.calls[0].count == 4, "device long arguments");
    device.shortBy = 1;
    Require(api.ReadUShortLE(&stream, &error) == 0 && error == InvalidStreamOperation && stream.pos == 4, "short device read of a short");
    device.shortBy = 0;
    device.calls.clear();
    stream.pos = 7;
    Require(api.ReadUShort(&stream, &error) == 0 && error == InvalidStreamOperation && device.calls.empty(), "short past the end does not call the device");
    stream.size = 0;
    stream.pos = 2;
    Require(api.ReadChar(&stream, &error) == 3 && error == 0 && stream.pos == 3, "device char does not check the size");
}

void TestCloseAndFree() {
    auto stream = DeviceStream({}, 0);
    api.Close(nullptr);
    api.Close(&stream);
    Require(device.closes == 0, "no close callback");
    stream.close = DeviceClose;
    api.Close(&stream);
    Require(device.closes == 1, "close callback");
    api.Free(nullptr, 0);
    stream.memory = &memory;
    api.Free(&stream, 1);
    Require(device.closes == 2 && device.releases.empty(), "external stream is closed, not released");
    auto* owned = static_cast<GuestStreamRec*>(std::malloc(sizeof(GuestStreamRec)));
    *owned = stream;
    owned->memory = &memory;
    device.order.clear();
    api.Free(owned, 0);
    Require(device.releases.size() == 1 && device.releases[0] == owned, "owned stream is released with the memory read before closing");
    Require(device.order.size() == 2 && device.order[0] == 'c' && device.order[1] == 'f', "close runs before release");
    stream.memory = nullptr;
    const auto closes = device.closes;
    Require(Throws([&] { api.Free(&stream, 0); }), "release without memory is unsupported");
    Require(device.closes == closes, "release without memory does not close the stream");
}

void TestReadFields() {
    std::array<std::uint8_t, 24> data{{0x7F, 0xFF, 0xFE, 0x80, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0xFF, 0x80, 0x00,
                                       0xAA, 'A', 'B', 'C', 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70}};
    auto stream = MemoryStream(data.data(), data.size());
    const std::array<GuestFrameField, 11> fields{{
        {4, 0, 17}, {8, 1, 0}, {9, 8, 8}, {12, 2, 16}, {15, 2, 18}, {17, 4, 20}, {21, 8, 24}, {25, 1, 0}, {24, 3, 32}, {13, 4, 36}, {0, 0, 0}}};
    std::array<std::uint8_t, 48> target{};
    target.fill(0xEE);
    Require(api.ReadFields(&stream, fields.data(), target.data()) == 0, "read fields");
    const auto load = [&](unsigned offset, unsigned size) {
        std::uint64_t value = 0;
        std::memcpy(&value, target.data() + offset, size);
        return value;
    };
    Require(load(0, 1) == 0x7F, "unsigned byte field");
    Require(load(8, 8) == Max, "signed byte field widened to eight bytes");
    Require(load(16, 2) == 0xFE80, "unsigned big-endian short field");
    Require(load(18, 2) == 0x8001, "signed little-endian short field");
    Require(load(20, 4) == 0x00000180u, "signed big-endian long field");
    Require(load(24, 8) == 0xFFFFFFFFFFFF8000u, "signed offset field widened to eight bytes");
    Require(target[32] == 'A' && target[33] == 'B' && target[34] == 'C' && target[35] == 0xEE, "byte sequence field");
    Require(load(36, 4) == 0x00001020u, "signed short field read past the frame");
    Require(!stream.cursor && !stream.limit && stream.pos == 17, "fields close their frame");
    const std::array<GuestFrameField, 3> tooLong{{{4, 0, 4}, {24, 5, 0}, {0, 0, 0}}};
    stream.pos = 0;
    Require(api.ReadFields(&stream, tooLong.data(), target.data()) == InvalidStreamOperation && !stream.cursor && stream.pos == 4, "byte sequence past the frame");
    const std::array<GuestFrameField, 2> stop{{{8, 1, 40}, {7, 0, 0}}};
    stream.cursor = data.data() + 3;
    stream.limit = data.data() + 4;
    Require(api.ReadFields(&stream, stop.data(), target.data()) == 0 && target[40] == 0x80 && stream.cursor == data.data() + 4, "unknown operation ends the list and keeps the cursor");
    stream.pos = 24;
    stream.cursor = nullptr;
    const std::array<GuestFrameField, 2> late{{{4, 0, 1}, {0, 0, 0}}};
    Require(api.ReadFields(&stream, late.data(), target.data()) == InvalidStreamOperation && stream.pos == 24, "frame start past the end");
    Require(api.ReadFields(nullptr, nullptr, nullptr) == InvalidArgument, "null fields");
    Require(Throws([&] { api.ReadFields(nullptr, fields.data(), target.data()); }), "null stream with fields is unsupported");
    const std::array<GuestFrameField, 2> noStructure{{{8, 1, 0}, {0, 0, 0}}};
    stream.cursor = data.data();
    stream.limit = data.data() + 1;
    Require(Throws([&] { api.ReadFields(&stream, noStructure.data(), nullptr); }), "null structure is unsupported");
    auto deviceStream = DeviceStream({0x12, 0x34, 0x56}, 3);
    const std::array<GuestFrameField, 3> deviceFields{{{4, 0, 3}, {20, 4, 0}, {0, 0, 0}}};
    Require(api.ReadFields(&deviceStream, deviceFields.data(), target.data()) == 0 && load(0, 4) == 0x123456, "device fields");
    Require(device.allocations.size() == 1 && device.releases.size() == 1 && !deviceStream.base && !deviceStream.cursor, "device fields release their frame");
    deviceStream = DeviceStream({0x12, 0x34, 0x56}, 3);
    const std::array<GuestFrameField, 3> deviceByte{{{4, 0, 3}, {8, 1, 0}, {0, 0, 0}}};
    Require(Throws([&] { api.ReadFields(&deviceStream, deviceByte.data(), nullptr); }), "null structure in a device frame is unsupported");
    Require(device.allocations.size() == 1 && device.releases.size() == 1, "refused fields release their device frame");
    Require(!deviceStream.base && !deviceStream.cursor && !deviceStream.limit && deviceStream.pos == 3, "refused fields leave no frame");
    stream.pos = 0;
    const std::array<GuestFrameField, 3> memoryBytes{{{4, 0, 4}, {24, 2, 0}, {0, 0, 0}}};
    Require(Throws([&] { api.ReadFields(&stream, memoryBytes.data(), nullptr); }), "null structure in a memory frame is unsupported");
    Require(!stream.cursor && !stream.limit && stream.base == data.data() && stream.pos == 4, "refused fields leave no memory frame");
}

void TestNullStream() {
    int error = 0;
    std::uint8_t* frame = nullptr;
    std::uint8_t byte = 0;
    Require(Throws([] { api.OpenMemory(nullptr, nullptr, 0); }), "open memory");
    Require(Throws([] { api.Seek(nullptr, 0); }) && Throws([] { api.Skip(nullptr, 0); }) && Throws([] { api.Pos(nullptr); }), "seek, skip and position");
    Require(Throws([&] { api.Read(nullptr, &byte, 1); }) && Throws([&] { api.ReadAt(nullptr, 0, &byte, 1); }) && Throws([&] { api.TryRead(nullptr, &byte, 1); }), "reads");
    Require(Throws([] { api.EnterFrame(nullptr, 0); }) && Throws([] { api.ExitFrame(nullptr); }) && Throws([&] { api.ExtractFrame(nullptr, 0, &frame); }), "frames");
    Require(Throws([] { api.GetChar(nullptr); }) && Throws([] { api.GetULong(nullptr); }) && Throws([] { api.GetOffset(nullptr); }), "getters");
    Require(Throws([&] { api.ReadChar(nullptr, &error); }) && Throws([&] { api.ReadLong(nullptr, &error); }), "readers");
}

void Run() {
    TestOpenMemoryAndPosition();
    TestMemoryReads();
    TestFrameGetters();
    TestMemoryReaders();
    TestDeviceStream();
    TestCloseAndFree();
    TestReadFields();
    TestNullStream();
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    Require(argc == 3, "expected library and export mode");
    const bool patched = std::strcmp(argv[2], "patched") == 0;
#ifdef _WIN32
    const auto library = LoadLibraryA(argv[1]);
    const auto symbol = [&](const char* name) { return reinterpret_cast<void*>(GetProcAddress(library, name)); };
#else
    const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
    const auto symbol = [&](const char* name) { return dlsym(library, name); };
#endif
    Require(library != nullptr, "load requested library");
#define RESOLVE(name, nid) \
    api.name = reinterpret_cast<decltype(api.name)>(symbol(patched ? nid : "FT_Stream_" #name "_nid_postfix")); \
    if (!api.name) std::fprintf(stderr, "Missing FT_Stream_%s\n", #name); \
    Require(api.name != nullptr, "missing stream export");
    STREAM_EXPORTS(RESOLVE)
#undef RESOLVE
    Run();
    std::puts("FreeType stream cases passed");
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
}
