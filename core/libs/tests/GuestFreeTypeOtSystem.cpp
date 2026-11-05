#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libSceFreeTypeOt/include/System.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

using FreeType::GuestMemoryRec;
using FreeType::GuestOpenArgs;
using FreeType::GuestRaccessLibrary;
using FreeType::GuestStreamRec;

constexpr int CannotOpenResource = 0x01;
constexpr int InvalidArgument = 0x06;
constexpr int InvalidLibraryHandle = 0x21;
constexpr int InvalidStreamHandle = 0x28;
constexpr int OutOfMemory = 0x40;
constexpr int CannotOpenStream = 0x51;
constexpr int InvalidStreamOperation = 0x55;

struct Recorder {
    GuestMemoryRec memory{};
    int allocs = 0;
    int frees = 0;
    int failAfter = -1;
};

void* APS5_VABI RecordAlloc(GuestMemoryRec* memory, std::int64_t size) {
    auto* recorder = static_cast<Recorder*>(memory->user);
    if (recorder->failAfter == 0) return nullptr;
    if (recorder->failAfter > 0) --recorder->failAfter;
    ++recorder->allocs;
    return std::malloc(static_cast<std::size_t>(size));
}

void APS5_VABI RecordFree(GuestMemoryRec* memory, void* block) {
    ++static_cast<Recorder*>(memory->user)->frees;
    std::free(block);
}

void* APS5_VABI RecordRealloc(GuestMemoryRec*, std::int64_t, std::int64_t size, void* block) {
    return std::realloc(block, static_cast<std::size_t>(size));
}

struct Library {
    Recorder recorder;
    GuestRaccessLibrary library{};

    Library() {
        recorder.memory = {&recorder, RecordAlloc, RecordFree, RecordRealloc};
        library.memory = &recorder.memory;
    }
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot system: %s\n", message);
    std::exit(1);
}

template <class Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void Write(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> Pattern(std::size_t size) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = static_cast<std::uint8_t>(index * 7 + 3);
    return bytes;
}

void OpenFile() {
    const auto bytes = Pattern(300);
    Write("app0/data.bin", bytes);
    Write("app0/empty.bin", {});
    std::filesystem::create_directories("app0/folder");

    Require(FT_Stream_Open_nid_postfix(nullptr, "/app0/data.bin") == InvalidStreamHandle, "a null stream is Invalid_Stream_Handle");

    GuestStreamRec stream{};
    std::memset(&stream, 0x5A, sizeof(stream));
    const char* missing = "/app0/missing.bin";
    Require(FT_Stream_Open_nid_postfix(&stream, missing) == CannotOpenResource, "a missing file is Cannot_Open_Resource");
    Require(stream.pathname == missing && !stream.descriptor && !stream.base && stream.pos == 0 && !stream.read && !stream.close,
            "a failed open still resets the stream and records the path");

    std::memset(&stream, 0x5A, sizeof(stream));
    Require(FT_Stream_Open_nid_postfix(&stream, "/app0/empty.bin") == CannotOpenStream, "an empty file is Cannot_Open_Stream");
    Require(stream.size == 0 && !stream.descriptor && !stream.read && !stream.close, "an empty file is closed again");

    Require(Throws([] {
        GuestStreamRec folder{};
        FT_Stream_Open_nid_postfix(&folder, "/app0/folder");
    }), "a directory throws");
    Require(Throws([] {
        GuestStreamRec unnamed{};
        FT_Stream_Open_nid_postfix(&unnamed, nullptr);
    }), "a null path throws");

    const char* path = "/app0/data.bin";
    Library owner;
    std::memset(&stream, 0x5A, sizeof(stream));
    auto* memory = &owner.recorder.memory;
    stream.memory = memory;
    Require(FT_Stream_Open_nid_postfix(&stream, path) == 0, "a regular file opens");
    Require(stream.size == 300 && stream.pos == 0 && !stream.base && stream.descriptor && stream.read && stream.close && stream.pathname == path,
            "an opened file is a callback stream at position 0");
    Require(stream.memory == memory, "the memory field is left to the caller");

    std::uint8_t buffer[16]{};
    Require(stream.read(&stream, 301, nullptr, 0) == 1 && stream.read(&stream, 300, nullptr, 0) == 0, "a seek past the end fails and a seek to the end succeeds");
    Require(FT_Stream_Seek_nid_postfix(&stream, 301) == InvalidStreamOperation && FT_Stream_Seek_nid_postfix(&stream, 300) == 0, "FT_Stream_Seek uses the file callback");
    Require(FT_Stream_ReadAt_nid_postfix(&stream, 290, buffer, 10) == 0 && std::memcmp(buffer, bytes.data() + 290, 10) == 0 && stream.pos == 300,
            "FT_Stream_ReadAt reads the file tail");
    Require(FT_Stream_ReadAt_nid_postfix(&stream, 295, buffer, 10) == InvalidStreamOperation && stream.pos == 300, "a short read is Invalid_Stream_Operation");
    Require(FT_Stream_Seek_nid_postfix(&stream, 4) == 0, "a seek back succeeds");
    int error = -1;
    const std::uint64_t value = FT_Stream_ReadULong_nid_postfix(&stream, &error);
    Require(error == 0 && value == (std::uint64_t{bytes[4]} << 24 | bytes[5] << 16 | bytes[6] << 8 | bytes[7]) && stream.pos == 8,
            "FT_Stream_ReadULong reads big-endian file bytes");
    Require(FT_Stream_EnterFrame_nid_postfix(&stream, 8) == 0, "a file frame is entered");
    Require(FT_Stream_GetUShort_nid_postfix(&stream) == (bytes[8] << 8 | bytes[9]), "a frame is read from the file");
    FT_Stream_ExitFrame_nid_postfix(&stream);
    Require(FT_Stream_TryRead_nid_postfix(&stream, buffer, 16) == 16 && std::memcmp(buffer, bytes.data() + 16, 16) == 0, "FT_Stream_TryRead continues after the frame");

    Require(owner.recorder.allocs == 1 && owner.recorder.frees == 1, "a file frame is allocated from the stream memory");
    FT_Stream_Close_nid_postfix(&stream);
    Require(!stream.descriptor && stream.size == 0 && !stream.base && stream.memory == memory, "closing releases the file and clears the size");
}

void NewStream() {
    Library owner;
    GuestOpenArgs args{};
    GuestStreamRec* stream = reinterpret_cast<GuestStreamRec*>(&args);

    Require(Throws([&] { FT_Stream_New_nid_postfix(&owner.library, &args, nullptr); }), "a null result pointer throws");
    Require(FT_Stream_New_nid_postfix(nullptr, &args, &stream) == InvalidLibraryHandle && !stream, "a null library is Invalid_Library_Handle");
    stream = reinterpret_cast<GuestStreamRec*>(&args);
    Require(FT_Stream_New_nid_postfix(&owner.library, nullptr, &stream) == InvalidArgument && !stream, "null arguments are Invalid_Argument");
    Require(FT_Stream_New_nid_postfix(&owner.library, &args, &stream) == InvalidArgument && !stream, "no source flag is Invalid_Argument");
    Require(owner.recorder.allocs == 1 && owner.recorder.frees == 1, "a rejected source frees its record");

    args.flags = 0x2;
    Require(FT_Stream_New_nid_postfix(&owner.library, &args, &stream) == InvalidArgument && !stream, "a stream flag without a stream is Invalid_Argument");

    const auto bytes = Pattern(32);
    args.flags = 0x1 | 0x4;
    args.memoryBase = bytes.data();
    args.memorySize = 32;
    args.pathname = const_cast<char*>("/app0/data.bin");
    Require(FT_Stream_New_nid_postfix(&owner.library, &args, &stream) == 0 && stream, "a memory source opens");
    Require(stream->base == bytes.data() && stream->size == 32 && !stream->read && !stream->pathname && stream->memory == &owner.recorder.memory,
            "the memory flag wins over the path flag");
    FT_Stream_Free_nid_postfix(stream, 0);
    Require(owner.recorder.allocs == owner.recorder.frees, "a memory stream is freed");

    args.flags = 0x4;
    Require(FT_Stream_New_nid_postfix(&owner.library, &args, &stream) == 0 && stream, "a path source opens");
    Require(stream->size == 300 && stream->read && stream->pathname == args.pathname && stream->memory == &owner.recorder.memory, "a path source is a file stream");
    std::uint8_t first[4]{};
    Require(FT_Stream_Read_nid_postfix(stream, first, 4) == 0 && first[0] == 3 && first[3] == 24, "the file stream reads the file");
    FT_Stream_Free_nid_postfix(stream, 0);
    Require(owner.recorder.allocs == owner.recorder.frees, "a file stream is closed and freed");

    args.pathname = const_cast<char*>("/app0/missing.bin");
    stream = reinterpret_cast<GuestStreamRec*>(&args);
    Require(FT_Stream_New_nid_postfix(&owner.library, &args, &stream) == CannotOpenResource && !stream, "a missing path fails");
    Require(owner.recorder.allocs == owner.recorder.frees, "a failed path frees its record");

    args.pathname = const_cast<char*>("/app0/folder");
    Require(Throws([&] { FT_Stream_New_nid_postfix(&owner.library, &args, &stream); }), "a directory path throws");
    Require(owner.recorder.allocs == owner.recorder.frees, "a throwing path frees its record");

    Recorder other;
    other.memory = {&other, RecordAlloc, RecordFree, RecordRealloc};
    GuestStreamRec external{};
    external.memory = &other.memory;
    args.flags = 0x2;
    args.stream = &external;
    Require(FT_Stream_New_nid_postfix(&owner.library, &args, &stream) == 0 && stream == &external, "a stream source returns the caller's stream");
    Require(external.memory == &owner.recorder.memory, "the caller's stream takes the library memory");
    Require(owner.recorder.allocs == owner.recorder.frees, "the unused record is freed");

    owner.recorder.failAfter = 0;
    args.flags = 0x1;
    stream = reinterpret_cast<GuestStreamRec*>(&args);
    Require(FT_Stream_New_nid_postfix(&owner.library, &args, &stream) == OutOfMemory && !stream, "a failed record allocation is Out_Of_Memory");
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5_freetype_ot_system-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    OpenFile();
    NewStream();
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    std::puts("Guest FreeType Ot system tests passed");
    return 0;
}
