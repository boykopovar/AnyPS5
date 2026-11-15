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
using FreeType::GuestStreamOpenArgs;
using FreeType::GuestRaccessLibrary;
using FreeType::GuestStreamRec;

constexpr int CannotOpenResource = 0x01;
constexpr int UnknownFileFormat = 0x02;
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

void Put16(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    bytes[at] = static_cast<std::uint8_t>(value >> 8);
    bytes[at + 1] = static_cast<std::uint8_t>(value);
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    Put16(bytes, at, value >> 16);
    Put16(bytes, at + 2, value);
}

std::vector<std::uint8_t> AppleHeader(std::uint32_t magic, std::uint32_t forkOffset, bool withFork = true) {
    std::vector<std::uint8_t> bytes(26 + 3 * 12 + 16, 0xA5);
    Put32(bytes, 0, magic);
    Put32(bytes, 4, 0x00020000);
    Put16(bytes, 24, 3);
    const std::uint32_t ids[3] = {9, withFork ? 2u : 3u, 1};
    for (std::size_t index = 0; index < 3; ++index) {
        Put32(bytes, 26 + index * 12, ids[index]);
        Put32(bytes, 30 + index * 12, ids[index] == 2 ? forkOffset : 0x1000 + static_cast<std::uint32_t>(index));
        Put32(bytes, 34 + index * 12, 0x20);
    }
    return bytes;
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
    GuestStreamOpenArgs args{};
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

struct Guess {
    char* names[9];
    std::int64_t offsets[9];
    std::int32_t errors[9];

    Guess() {
        for (int index = 0; index < 9; ++index) {
            names[index] = reinterpret_cast<char*>(1);
            offsets[index] = -7;
            errors[index] = -1;
        }
    }
};

bool Named(const char* name, const char* expected) {
    return name && std::strcmp(name, expected) == 0;
}

void GuessForks() {
    Write("app0/fonts/font.dfont", AppleHeader(0x00051600, 0x52));
    Write("app0/fonts/._font.dfont", AppleHeader(0x00051607, 0x1234));
    Write("app0/fonts/%font.dfont", AppleHeader(0x00051607, 0x80000000u));
    Write("app0/fonts/.AppleDouble/font.dfont", AppleHeader(0x00051607, 0x99, false));

    Library owner;
    GuestStreamRec stream{};
    Require(FT_Stream_Open_nid_postfix(&stream, "/app0/fonts/font.dfont") == 0, "the base file opens");
    FT_Stream_Seek_nid_postfix(&stream, 20);
    char base[] = "/app0/fonts/font.dfont";
    Guess guess;
    FT_Raccess_Guess_nid_postfix(&owner.library, &stream, base, guess.names, guess.offsets, guess.errors);
    Require(guess.errors[0] == UnknownFileFormat && !guess.names[0] && guess.offsets[0] == -7, "AppleDouble rejects an AppleSingle magic");
    Require(guess.errors[1] == 0 && !guess.names[1] && guess.offsets[1] == 0x52, "AppleSingle finds the fork entry after a rewind");
    Require(guess.errors[2] == 0 && Named(guess.names[2], "/app0/fonts/._font.dfont") && guess.offsets[2] == 0x1234, "the darwin export file is read");
    Require(guess.errors[3] == 0 && Named(guess.names[3], "/app0/fonts/font.dfont/..namedfork/rsrc") && guess.offsets[3] == 0, "the darwin named fork is appended");
    Require(guess.errors[4] == 0 && Named(guess.names[4], "/app0/fonts/font.dfont/rsrc") && guess.offsets[4] == 0, "the HFS+ fork is appended");
    Require(guess.errors[5] == 0 && Named(guess.names[5], "/app0/fonts/resource.frk/font.dfont") && guess.offsets[5] == 0, "the vfat name is inserted");
    Require(guess.errors[6] == 0 && Named(guess.names[6], "/app0/fonts/.resource/font.dfont") && guess.offsets[6] == 0, "the CAP name is inserted");
    Require(guess.errors[7] == 0 && Named(guess.names[7], "/app0/fonts/%font.dfont") && guess.offsets[7] == 0x80000000ll, "the double file offset is unsigned");
    Require(guess.errors[8] == UnknownFileFormat && !guess.names[8] && guess.offsets[8] == -7, "a double file without a fork entry fails");
    for (int index = 0; index < 9; ++index) ft_mem_free_nid_postfix(&owner.recorder.memory, guess.names[index]);
    Require(owner.recorder.allocs == owner.recorder.frees, "rejected names and probe streams are freed");

    Guess noStream;
    char plain[] = "font.dfont";
    FT_Raccess_Guess_nid_postfix(&owner.library, nullptr, plain, noStream.names, noStream.offsets, noStream.errors);
    Require(noStream.errors[0] == CannotOpenStream && noStream.errors[1] == CannotOpenStream && !noStream.names[0] && !noStream.names[1],
            "the Apple rules need a stream");
    Require(noStream.errors[2] == CannotOpenResource && noStream.errors[7] == CannotOpenResource && noStream.errors[8] == CannotOpenResource,
            "missing double files are Cannot_Open_Resource");
    Require(Named(noStream.names[3], "font.dfont/..namedfork/rsrc") && Named(noStream.names[5], "resource.frk/font.dfont") &&
                Named(noStream.names[6], ".resource/font.dfont"),
            "a name without a directory gets the insertion first");
    for (int index = 0; index < 9; ++index) ft_mem_free_nid_postfix(&owner.recorder.memory, noStream.names[index]);

    std::vector<std::uint8_t> truncated = AppleHeader(0x00051600, 0x52);
    truncated.resize(30);
    std::vector<std::uint8_t> empty = AppleHeader(0x00051600, 0x52);
    Put16(empty, 24, 0);
    for (const auto& [bytes, expected] : {std::pair{truncated, InvalidStreamOperation}, std::pair{empty, UnknownFileFormat}}) {
        GuestStreamRec memoryStream{};
        FT_Stream_OpenMemory_nid_postfix(&memoryStream, bytes.data(), bytes.size());
        Guess result;
        FT_Raccess_Guess_nid_postfix(&owner.library, &memoryStream, plain, result.names, result.offsets, result.errors);
        Require(result.errors[1] == expected && result.offsets[1] == -7, "a cut or empty entry list fails");
        for (int index = 0; index < 9; ++index) ft_mem_free_nid_postfix(&owner.recorder.memory, result.names[index]);
    }

    owner.recorder.failAfter = 0;
    Guess starved;
    FT_Raccess_Guess_nid_postfix(&owner.library, &stream, base, starved.names, starved.offsets, starved.errors);
    for (int index = 2; index < 9; ++index) Require(starved.errors[index] == OutOfMemory && !starved.names[index], "a failed name allocation is Out_Of_Memory");
    owner.recorder.failAfter = -1;

    Require(Throws([&] {
        Guess unnamed;
        FT_Raccess_Guess_nid_postfix(&owner.library, &stream, nullptr, unnamed.names, unnamed.offsets, unnamed.errors);
    }), "a null base name throws");
    Require(Throws([&] {
        Guess orphan;
        FT_Raccess_Guess_nid_postfix(nullptr, &stream, base, orphan.names, orphan.offsets, orphan.errors);
    }), "a null library throws");
    FT_Stream_Close_nid_postfix(&stream);
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5_freetype_ot_system-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    OpenFile();
    NewStream();
    GuessForks();
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    std::puts("Guest FreeType Ot system tests passed");
    return 0;
}
