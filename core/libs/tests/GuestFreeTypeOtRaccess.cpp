#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libSceFreeTypeOt/include/Raccess.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using FreeType::GuestMemoryRec;
using FreeType::GuestRaccessLibrary;
using FreeType::GuestStreamRec;

constexpr std::int64_t Post = 0x504F5354;
constexpr std::int64_t Sfnt = 0x73666E74;

struct Recorder {
    GuestMemoryRec memory{};
    int allocs = 0;
    int frees = 0;
};

void* APS5_VABI RecordAlloc(GuestMemoryRec* memory, std::int64_t size) {
    ++static_cast<Recorder*>(memory->user)->allocs;
    return std::malloc(static_cast<std::size_t>(size));
}

void APS5_VABI RecordFree(GuestMemoryRec* memory, void* block) {
    ++static_cast<Recorder*>(memory->user)->frees;
    std::free(block);
}

void* APS5_VABI RecordRealloc(GuestMemoryRec*, std::int64_t, std::int64_t size, void* block) {
    return std::realloc(block, static_cast<std::size_t>(size));
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot raccess: %s\n", message);
    std::exit(1);
}

void Put16(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    bytes[at] = static_cast<std::uint8_t>(value >> 8);
    bytes[at + 1] = static_cast<std::uint8_t>(value);
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    Put16(bytes, at, value >> 16);
    Put16(bytes, at + 2, value);
}

struct Fork {
    std::vector<std::uint8_t> bytes;
    std::size_t prefix;
    std::uint32_t dataStart = 0x100;
    std::uint32_t dataLength = 0x40;
    std::uint32_t mapPos = 0x140;
    std::uint32_t typeList = 28;
};

Fork MakeFork(std::size_t prefix, bool zeroCopy) {
    Fork fork;
    fork.prefix = prefix;
    fork.bytes.assign(prefix + 0x140 + 0x70, 0xEE);
    const std::size_t base = prefix;
    Put32(fork.bytes, base, fork.dataStart);
    Put32(fork.bytes, base + 4, fork.mapPos);
    Put32(fork.bytes, base + 8, fork.dataLength);
    Put32(fork.bytes, base + 12, 0x60);
    const std::size_t map = base + fork.mapPos;
    for (std::size_t index = 0; index < 16; ++index) fork.bytes[map + index] = zeroCopy ? 0 : fork.bytes[base + index];
    Put16(fork.bytes, map + 24, fork.typeList);
    const std::size_t types = map + fork.typeList;
    Put16(fork.bytes, types, 1);
    Put32(fork.bytes, types + 2, Sfnt);
    Put16(fork.bytes, types + 6, 0);
    Put16(fork.bytes, types + 8, 22);
    Put32(fork.bytes, types + 10, Post);
    Put16(fork.bytes, types + 14, 2);
    Put16(fork.bytes, types + 16, 34);
    const std::uint32_t refs[4][2] = {{0x0080, 0x00000010}, {0x0102, 0xA0000020}, {0x0100, 0x00000030}, {0x0101, 0x7F000008}};
    for (std::size_t index = 0; index < 4; ++index) {
        const std::size_t ref = types + 22 + index * 12;
        Put16(fork.bytes, ref, refs[index][0]);
        Put16(fork.bytes, ref + 2, 0xFFFF);
        Put32(fork.bytes, ref + 4, refs[index][1]);
        Put32(fork.bytes, ref + 8, 0);
    }
    return fork;
}

struct Session {
    Recorder recorder;
    GuestRaccessLibrary library{};
    GuestStreamRec stream{};

    explicit Session(const std::vector<std::uint8_t>& bytes) {
        recorder.memory = {&recorder, RecordAlloc, RecordFree, RecordRealloc};
        library.memory = &recorder.memory;
        FT_Stream_OpenMemory_nid_postfix(&stream, bytes.data(), bytes.size());
    }
};

void HeaderAndOffsets(std::size_t prefix, bool zeroCopy) {
    const Fork fork = MakeFork(prefix, zeroCopy);
    Session session(fork.bytes);
    std::int64_t mapOffset = -1;
    std::int64_t rdataPos = -1;
    const auto offset = static_cast<std::int64_t>(prefix);
    Require(FT_Raccess_Get_HeaderInfo_nid_postfix(&session.library, &session.stream, offset, &mapOffset, &rdataPos) == 0, "a resource fork header is accepted");
    Require(rdataPos == offset + 0x100 && mapOffset == offset + 0x140 + 28, "header info adds the fork offset to the data and type list positions");
    Require(session.stream.pos == static_cast<std::uint64_t>(mapOffset), "header info leaves the stream at the type list");

    std::int64_t* offsets = nullptr;
    std::int64_t count = 0;
    Require(FT_Raccess_Get_DataOffsets_nid_postfix(&session.library, &session.stream, mapOffset, rdataPos, Post, &offsets, &count) == 0, "a present tag is found");
    Require(count == 3 && offsets, "the subcount plus one references are returned");
    Require(offsets[0] == rdataPos + 0x30 && offsets[1] == rdataPos + 0x08 && offsets[2] == rdataPos + 0x20, "offsets are sorted by resource ID and drop the attribute byte");
    Require(session.recorder.allocs == 2 && session.recorder.frees == 1, "the reference array is freed and the offsets array is kept");
    ft_mem_free_nid_postfix(&session.recorder.memory, offsets);

    offsets = nullptr;
    count = 0;
    Require(FT_Raccess_Get_DataOffsets_nid_postfix(&session.library, &session.stream, mapOffset, rdataPos, Sfnt, &offsets, &count) == 0, "the first type is found");
    Require(count == 1 && offsets[0] == rdataPos + 0x10, "a single reference is returned");
    ft_mem_free_nid_postfix(&session.recorder.memory, offsets);

    offsets = nullptr;
    count = -1;
    const int allocs = session.recorder.allocs;
    Require(FT_Raccess_Get_DataOffsets_nid_postfix(&session.library, &session.stream, mapOffset, rdataPos, 0x4E464E54, &offsets, &count) == 0x01, "a missing tag is Cannot_Open_Resource");
    Require(count == -1 && !offsets && session.recorder.allocs == allocs, "a missing tag writes and allocates nothing");
}

void RejectedHeaders() {
    Fork fork = MakeFork(0, false);
    Put32(fork.bytes, 8, 0x41);
    Session lengths(fork.bytes);
    std::int64_t mapOffset = -1;
    std::int64_t rdataPos = -1;
    Require(FT_Raccess_Get_HeaderInfo_nid_postfix(&lengths.library, &lengths.stream, 0, &mapOffset, &rdataPos) == 0x02, "data that does not end at the map is Unknown_File_Format");
    Require(rdataPos == 0x100 && mapOffset == -1, "the data position is written before the length check");

    fork = MakeFork(0, false);
    fork.bytes[0x140 + 3] ^= 1;
    Session copy(fork.bytes);
    rdataPos = -1;
    Require(FT_Raccess_Get_HeaderInfo_nid_postfix(&copy.library, &copy.stream, 0, &mapOffset, &rdataPos) == 0x02, "a map header that neither matches nor is zero is Unknown_File_Format");

    fork = MakeFork(0, false);
    Put32(fork.bytes, 0, 0);
    Put32(fork.bytes, 4, 0);
    Put32(fork.bytes, 8, 0);
    Session empty(fork.bytes);
    Require(FT_Raccess_Get_HeaderInfo_nid_postfix(&empty.library, &empty.stream, 0, &mapOffset, &rdataPos) == 0x02, "a map at the fork offset is Unknown_File_Format");

    const std::vector<std::uint8_t> shortStream(10, 0);
    Session truncated(shortStream);
    Require(FT_Raccess_Get_HeaderInfo_nid_postfix(&truncated.library, &truncated.stream, 0, &mapOffset, &rdataPos) == 0x55, "a short header is Invalid_Stream_Operation");
    Require(FT_Raccess_Get_HeaderInfo_nid_postfix(&truncated.library, &truncated.stream, 11, &mapOffset, &rdataPos) == 0x55, "a fork offset past the stream is Invalid_Stream_Operation");
}

void TruncatedReferences() {
    Fork fork = MakeFork(0, false);
    fork.bytes.resize(0x140 + 28 + 22 + 12 + 6);
    Session session(fork.bytes);
    std::int64_t* offsets = nullptr;
    std::int64_t count = 0;
    Require(FT_Raccess_Get_DataOffsets_nid_postfix(&session.library, &session.stream, 0x140 + 28, 0x100, Post, &offsets, &count) == 0x55, "a cut reference list is Invalid_Stream_Operation");
    Require(count == 3 && !offsets, "the count is written before the references are read");
    Require(session.recorder.allocs == 1 && session.recorder.frees == 1, "the reference array is freed on a read error");
}

template <class F>
bool RejectsKeepingErrno(F call) {
    errno = 71;
    try {
        call();
    } catch (const std::exception&) {
        return errno == 71;
    }
    return false;
}

std::size_t PageSize() {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    const long size = sysconf(_SC_PAGESIZE);
    return size > 0 ? static_cast<std::size_t>(size) : 4096;
#endif
}

char* MapPages(std::size_t bytes) {
#ifdef _WIN32
    void* pages = VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(pages != nullptr, "pages are mapped");
#else
    void* pages = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(pages != MAP_FAILED, "pages are mapped");
#endif
    return static_cast<char*>(pages);
}

void Protect(char* page, std::size_t bytes, bool readable) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, bytes, readable ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0, "a page is protected");
#else
    Require(mprotect(page, bytes, readable ? PROT_READ : PROT_NONE) == 0, "a page is protected");
#endif
}

void UnmapPages(char* pages, std::size_t bytes) {
#ifdef _WIN32
    (void)bytes;
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, bytes);
#endif
}

void GuessValidAndPageEnd() {
    const std::size_t pageSize = PageSize();
    char* pages = MapPages(2 * pageSize);
    char* guard = pages + pageSize;
    constexpr char baseLiteral[] = "dir/font";
    char* baseAtEnd = guard - sizeof(baseLiteral);
    std::memcpy(baseAtEnd, baseLiteral, sizeof(baseLiteral));
    Protect(guard, pageSize, false);

    const Fork fork = MakeFork(0, false);
    Session session(fork.bytes);
    std::array<char*, 9> newNames{};
    std::array<std::int64_t, 9> offsets{};
    offsets.fill(-1);
    std::array<std::int32_t, 9> errors{};
    errors.fill(-1);
    FT_Raccess_Guess_nid_postfix(&session.library, nullptr, baseAtEnd, newNames.data(), offsets.data(), errors.data());

    Require(errors[0] == 0x51 && !newNames[0] && offsets[0] == -1, "rule 0 with null stream leaves offset untouched");
    Require(errors[1] == 0x51 && !newNames[1] && offsets[1] == -1, "rule 1 with null stream leaves offset untouched");
    Require(errors[2] != 0 && !newNames[2] && offsets[2] == -1, "rule 2 cleans up failed apple double guess");
    Require(errors[3] == 0 && newNames[3] && std::strcmp(newNames[3], "dir/font/..namedfork/rsrc") == 0 && offsets[3] == 0, "rule 3 appends namedfork suffix");
    Require(errors[4] == 0 && newNames[4] && std::strcmp(newNames[4], "dir/font/rsrc") == 0 && offsets[4] == 0, "rule 4 appends rsrc suffix");
    Require(errors[5] == 0 && newNames[5] && std::strcmp(newNames[5], "dir/resource.frk/font") == 0 && offsets[5] == 0, "rule 5 inserts resource.frk");
    Require(errors[6] == 0 && newNames[6] && std::strcmp(newNames[6], "dir/.resource/font") == 0 && offsets[6] == 0, "rule 6 inserts .resource");
    Require(errors[7] != 0 && !newNames[7] && offsets[7] == -1, "rule 7 cleans up failed apple double guess");
    Require(errors[8] != 0 && !newNames[8] && offsets[8] == -1, "rule 8 cleans up failed apple double guess");

    for (char* name : newNames) {
        if (name) ft_mem_free_nid_postfix(&session.recorder.memory, name);
    }
    Require(session.recorder.allocs == session.recorder.frees, "all allocated names in guess are freed");
    UnmapPages(pages, 2 * pageSize);
}

void GuestPointers() {
    const std::size_t pageSize = PageSize();
    char* pages = MapPages(3 * pageSize);
    char* readOnly = pages + pageSize;
    char* guard = pages + 2 * pageSize;
    std::memset(readOnly, 0x5A, pageSize);
    char* unterminatedTail = guard - 4;
    std::memcpy(unterminatedTail, "font", 4);
    Protect(readOnly, pageSize, true);
    Protect(guard, pageSize, false);

    auto* unmappedLong = reinterpret_cast<std::int64_t*>(std::uintptr_t{0x10});
    auto* readOnlyLong = reinterpret_cast<std::int64_t*>(readOnly);
    const std::int64_t readOnlySentinel = *readOnlyLong;

    const Fork fork = MakeFork(0, false);
    {
        Session session(fork.bytes);
        std::int64_t mapOffset = -1;
        Require(RejectsKeepingErrno([&] { FT_Raccess_Get_HeaderInfo_nid_postfix(&session.library, &session.stream, 0, &mapOffset, unmappedLong); }),
                "header info refuses an unmapped rdataPos");
        Require(mapOffset == -1, "unmapped rdataPos fails before writing mapOffset");
    }
    {
        Session session(fork.bytes);
        std::int64_t mapOffset = -1;
        Require(RejectsKeepingErrno([&] { FT_Raccess_Get_HeaderInfo_nid_postfix(&session.library, &session.stream, 0, &mapOffset, readOnlyLong); }),
                "header info refuses a read-only rdataPos");
        Require(*readOnlyLong == readOnlySentinel, "read-only rdataPos is untouched");
    }
    {
        Session session(fork.bytes);
        std::int64_t rdataPos = -1;
        Require(RejectsKeepingErrno([&] { FT_Raccess_Get_HeaderInfo_nid_postfix(&session.library, &session.stream, 0, unmappedLong, &rdataPos); }),
                "header info refuses an unmapped mapOffset");
    }
    {
        Session session(fork.bytes);
        std::int64_t rdataPos = -1;
        Require(RejectsKeepingErrno([&] { FT_Raccess_Get_HeaderInfo_nid_postfix(&session.library, &session.stream, 0, readOnlyLong, &rdataPos); }),
                "header info refuses a read-only mapOffset");
        Require(*readOnlyLong == readOnlySentinel, "read-only mapOffset is untouched");
    }

    Session session(fork.bytes);
    auto* unmappedLibrary = reinterpret_cast<GuestRaccessLibrary*>(std::uintptr_t{0x10});
    GuestRaccessLibrary unmappedMemoryLibrary{};
    unmappedMemoryLibrary.memory = reinterpret_cast<GuestMemoryRec*>(std::uintptr_t{0x10});
    GuestRaccessLibrary nullMemoryLibrary{};
    auto* unmappedName = reinterpret_cast<char*>(std::uintptr_t{0x10});
    char mutableBase[] = "demo";
    std::array<char*, 9> newNames{};
    std::array<std::int64_t, 9> offsets{};
    std::array<std::int32_t, 9> errors{};

    Require(RejectsKeepingErrno([&] {
                FT_Raccess_Guess_nid_postfix(unmappedLibrary, nullptr, mutableBase, newNames.data(), offsets.data(), errors.data());
            }),
            "guess refuses an unmapped library");
    Require(RejectsKeepingErrno([&] {
                FT_Raccess_Guess_nid_postfix(&nullMemoryLibrary, nullptr, mutableBase, newNames.data(), offsets.data(), errors.data());
            }),
            "guess refuses a null library memory");
    Require(RejectsKeepingErrno([&] {
                FT_Raccess_Guess_nid_postfix(&unmappedMemoryLibrary, nullptr, mutableBase, newNames.data(), offsets.data(), errors.data());
            }),
            "guess refuses an unmapped library memory");
    Require(RejectsKeepingErrno([&] {
                FT_Raccess_Guess_nid_postfix(&session.library, nullptr, unmappedName, newNames.data(), offsets.data(), errors.data());
            }),
            "guess refuses an unmapped baseName");
    Require(RejectsKeepingErrno([&] {
                FT_Raccess_Guess_nid_postfix(&session.library, nullptr, unterminatedTail, newNames.data(), offsets.data(), errors.data());
            }),
            "guess refuses an unterminated baseName hitting a guard page");
    Require(RejectsKeepingErrno([&] {
                FT_Raccess_Guess_nid_postfix(&session.library, nullptr, mutableBase, reinterpret_cast<char**>(readOnly), offsets.data(), errors.data());
            }),
            "guess refuses a read-only newNames array");
    Require(RejectsKeepingErrno([&] {
                FT_Raccess_Guess_nid_postfix(&session.library, nullptr, mutableBase, reinterpret_cast<char**>(std::uintptr_t{0x10}), offsets.data(), errors.data());
            }),
            "guess refuses an unmapped newNames array");
    Require(RejectsKeepingErrno([&] {
                FT_Raccess_Guess_nid_postfix(&session.library, nullptr, mutableBase, newNames.data(), readOnlyLong, errors.data());
            }),
            "guess refuses a read-only offsets array");
    Require(RejectsKeepingErrno([&] {
                FT_Raccess_Guess_nid_postfix(&session.library, nullptr, mutableBase, newNames.data(), unmappedLong, errors.data());
            }),
            "guess refuses an unmapped offsets array");
    Require(RejectsKeepingErrno([&] {
                FT_Raccess_Guess_nid_postfix(&session.library, nullptr, mutableBase, newNames.data(), offsets.data(), reinterpret_cast<std::int32_t*>(readOnly));
            }),
            "guess refuses a read-only errors array");
    Require(RejectsKeepingErrno([&] {
                FT_Raccess_Guess_nid_postfix(&session.library, nullptr, mutableBase, newNames.data(), offsets.data(), reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10}));
            }),
            "guess refuses an unmapped errors array");
    Require(session.recorder.allocs == 0 && *readOnlyLong == readOnlySentinel, "rejected guess calls allocate nothing and keep read-only memory untouched");

    UnmapPages(pages, 3 * pageSize);
}

}

int main() {
    HeaderAndOffsets(0, false);
    HeaderAndOffsets(0x80, true);
    RejectedHeaders();
    TruncatedReferences();
    GuessValidAndPageEnd();
    GuestPointers();
    std::puts("Guest FreeType Ot raccess tests passed");
    return 0;
}
