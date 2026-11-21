#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libSceFreeTypeOt/include/Outline.hpp"
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <linux/ioctl.h>
#include <sys/syscall.h>
#endif

#ifdef __linux__
std::atomic<int> procmapQueries{0};

extern "C" int ioctl(int fd, unsigned long request, ...) {
    va_list arguments;
    va_start(arguments, request);
    void* argument = va_arg(arguments, void*);
    va_end(arguments);
    if (request == _IOWR('f', 17, char[104])) ++procmapQueries;
    return static_cast<int>(syscall(SYS_ioctl, fd, request, argument));
}
#endif

namespace {

using FreeType::GuestMemoryRec;

struct Recorder {
    GuestMemoryRec memory{};
    int allocs = 0;
    int frees = 0;
    int reallocs = 0;
    std::int64_t lastSize = 0;
    std::int64_t lastCurrent = 0;
    std::int64_t lastNew = 0;
    void* lastBlock = nullptr;
    GuestMemoryRec* lastMemory = nullptr;
    bool fail = false;
};

Recorder* Owner(GuestMemoryRec* memory) {
    return static_cast<Recorder*>(memory->user);
}

void* APS5_VABI RecordAlloc(GuestMemoryRec* memory, std::int64_t size) {
    auto* recorder = Owner(memory);
    ++recorder->allocs;
    recorder->lastMemory = memory;
    recorder->lastSize = size;
    if (recorder->fail) return nullptr;
    void* block = std::malloc(static_cast<std::size_t>(size));
    std::memset(block, 0xAA, static_cast<std::size_t>(size));
    return block;
}

void APS5_VABI RecordFree(GuestMemoryRec* memory, void* block) {
    auto* recorder = Owner(memory);
    ++recorder->frees;
    recorder->lastMemory = memory;
    recorder->lastBlock = block;
    std::free(block);
}

void* APS5_VABI RecordRealloc(GuestMemoryRec* memory, std::int64_t current, std::int64_t size, void* block) {
    auto* recorder = Owner(memory);
    ++recorder->reallocs;
    recorder->lastMemory = memory;
    recorder->lastCurrent = current;
    recorder->lastNew = size;
    recorder->lastBlock = block;
    if (recorder->fail) return nullptr;
    auto* resized = static_cast<unsigned char*>(std::realloc(block, static_cast<std::size_t>(size)));
    if (size > current) std::memset(resized + current, 0xAA, static_cast<std::size_t>(size - current));
    return resized;
}

void Reset(Recorder& recorder) {
    recorder = Recorder{};
    recorder.memory = {&recorder, RecordAlloc, RecordFree, RecordRealloc};
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot memory: %s\n", message);
    std::exit(1);
}

bool Filled(const void* block, std::size_t size, unsigned char value) {
    const auto* bytes = static_cast<const unsigned char*>(block);
    for (std::size_t i = 0; i < size; ++i) {
        if (bytes[i] != value) return false;
    }
    return true;
}

void QuickAllocation() {
    Recorder recorder;
    Reset(recorder);
    std::int32_t error = -1;
    void* block = ft_mem_qalloc_nid_postfix(&recorder.memory, 24, &error);
    Require(block && error == 0, "qalloc of a positive size returns a block and no error");
    Require(recorder.allocs == 1 && recorder.lastSize == 24 && recorder.lastMemory == &recorder.memory, "qalloc passes the memory record and size to alloc");
    Require(Filled(block, 24, 0xAA), "qalloc leaves the block uncleared");
    ft_mem_free_nid_postfix(&recorder.memory, block);
    Require(recorder.frees == 1 && recorder.lastBlock == block && recorder.lastMemory == &recorder.memory, "free passes the block to memory->free");

    Reset(recorder);
    error = -1;
    Require(!ft_mem_qalloc_nid_postfix(&recorder.memory, 0, &error) && error == 0 && recorder.allocs == 0, "qalloc of size 0 returns null without calling alloc");
    error = -1;
    Require(!ft_mem_qalloc_nid_postfix(&recorder.memory, -1, &error) && error == 0x06 && recorder.allocs == 0, "qalloc of a negative size is Invalid_Argument");
    recorder.fail = true;
    error = -1;
    Require(!ft_mem_qalloc_nid_postfix(&recorder.memory, 8, &error) && error == 0x40 && recorder.allocs == 1, "qalloc reports Out_Of_Memory when alloc fails");
}

void ClearedAllocation() {
    Recorder recorder;
    Reset(recorder);
    std::int32_t error = -1;
    void* block = ft_mem_alloc_nid_postfix(&recorder.memory, 16, &error);
    Require(block && error == 0 && recorder.allocs == 1 && recorder.lastSize == 16, "alloc allocates through memory->alloc");
    Require(Filled(block, 16, 0), "alloc clears the block");
    ft_mem_free_nid_postfix(&recorder.memory, block);

    error = -1;
    Require(!ft_mem_alloc_nid_postfix(&recorder.memory, 0, &error) && error == 0, "alloc of size 0 returns null");
    error = -1;
    Require(!ft_mem_alloc_nid_postfix(&recorder.memory, -5, &error) && error == 0x06, "alloc of a negative size is Invalid_Argument");
    recorder.fail = true;
    error = -1;
    Require(!ft_mem_alloc_nid_postfix(&recorder.memory, 4, &error) && error == 0x40, "alloc reports Out_Of_Memory when alloc fails");

    Reset(recorder);
    ft_mem_free_nid_postfix(&recorder.memory, nullptr);
    Require(recorder.frees == 0, "free of null does not call memory->free");
}

void QuickReallocation() {
    Recorder recorder;
    Reset(recorder);
    std::int32_t error = -1;
    void* block = ft_mem_qrealloc_nid_postfix(&recorder.memory, 4, 0, 3, nullptr, &error);
    Require(block && error == 0 && recorder.allocs == 1 && recorder.lastSize == 12 && recorder.reallocs == 0, "qrealloc from zero items allocates new_count * item_size");
    Require(Filled(block, 12, 0), "qrealloc from zero items clears the new block, as ft_mem_alloc does");
    std::memset(block, 0x11, 12);

    error = -1;
    void* grown = ft_mem_qrealloc_nid_postfix(&recorder.memory, 4, 3, 5, block, &error);
    Require(grown && error == 0 && recorder.reallocs == 1, "qrealloc of a live block calls memory->realloc");
    Require(recorder.lastCurrent == 12 && recorder.lastNew == 20 && recorder.lastBlock == block, "qrealloc passes byte sizes and the old block");
    Require(Filled(grown, 12, 0x11) && Filled(static_cast<char*>(grown) + 12, 8, 0xAA), "qrealloc does not clear the grown tail");

    recorder.fail = true;
    error = -1;
    Require(ft_mem_qrealloc_nid_postfix(&recorder.memory, 4, 5, 9, grown, &error) == grown && error == 0x40, "qrealloc keeps the old block when realloc fails");
    recorder.fail = false;

    const int callbacks = recorder.allocs + recorder.frees + recorder.reallocs;
    error = -1;
    Require(ft_mem_qrealloc_nid_postfix(&recorder.memory, -1, 5, 6, grown, &error) == grown && error == 0x06, "negative item_size is Invalid_Argument");
    error = -1;
    Require(ft_mem_qrealloc_nid_postfix(&recorder.memory, 4, -1, 6, grown, &error) == grown && error == 0x06, "negative cur_count is Invalid_Argument");
    error = -1;
    Require(ft_mem_qrealloc_nid_postfix(&recorder.memory, 4, 5, -1, grown, &error) == grown && error == 0x06, "negative new_count is Invalid_Argument");
    error = -1;
    Require(ft_mem_qrealloc_nid_postfix(&recorder.memory, 2, 5, 0x40000000, grown, &error) == grown && error == 0x0A, "new_count above FT_INT_MAX / item_size is Array_Too_Large");
    Require(recorder.allocs + recorder.frees + recorder.reallocs == callbacks, "rejected qrealloc calls make no memory callback");

    recorder.fail = true;
    error = -1;
    Require(!ft_mem_qrealloc_nid_postfix(&recorder.memory, 2, 0, 0x3FFFFFFF, nullptr, &error) && error == 0x40, "new_count at FT_INT_MAX / item_size is allocated");
    Require(recorder.lastSize == 0x7FFFFFFE, "the boundary allocation asks for new_count * item_size bytes");
    recorder.fail = false;

    error = -1;
    Require(!ft_mem_qrealloc_nid_postfix(&recorder.memory, 4, 5, 0, grown, &error) && error == 0, "qrealloc to zero items frees and returns null");
    Require(recorder.lastBlock == grown && recorder.frees == 1, "qrealloc to zero items passes the block to memory->free");

    block = ft_mem_qalloc_nid_postfix(&recorder.memory, 8, &error);
    error = -1;
    Require(!ft_mem_qrealloc_nid_postfix(&recorder.memory, 0, 2, 4, block, &error) && error == 0, "qrealloc with item_size 0 frees and returns null");
    Require(recorder.lastBlock == block && recorder.frees == 2, "qrealloc with item_size 0 passes the block to memory->free");
}

void ClearedReallocation() {
    Recorder recorder;
    Reset(recorder);
    std::int32_t error = -1;
    void* block = ft_mem_realloc_nid_postfix(&recorder.memory, 4, 0, 3, nullptr, &error);
    Require(block && error == 0 && recorder.lastSize == 12 && Filled(block, 12, 0), "realloc from zero items allocates and clears");
    auto* bytes = static_cast<unsigned char*>(block);
    for (int i = 0; i < 12; ++i) bytes[i] = static_cast<unsigned char>(i + 1);

    error = -1;
    void* grown = ft_mem_realloc_nid_postfix(&recorder.memory, 4, 3, 5, block, &error);
    Require(grown && error == 0 && recorder.reallocs == 1 && recorder.lastCurrent == 12 && recorder.lastNew == 20, "realloc resizes through memory->realloc");
    bytes = static_cast<unsigned char*>(grown);
    bool kept = true;
    for (int i = 0; i < 12; ++i) kept = kept && bytes[i] == i + 1;
    Require(kept && Filled(bytes + 12, 8, 0), "realloc keeps the old items and clears the new ones");

    std::memset(bytes, 0x22, 20);
    error = -1;
    void* shrunk = ft_mem_realloc_nid_postfix(&recorder.memory, 4, 5, 2, grown, &error);
    Require(shrunk && error == 0 && recorder.lastCurrent == 20 && recorder.lastNew == 8 && Filled(shrunk, 8, 0x22), "realloc shrinks without clearing");

    error = -1;
    Require(ft_mem_realloc_nid_postfix(&recorder.memory, 4, 2, -3, shrunk, &error) == shrunk && error == 0x06 && Filled(shrunk, 8, 0x22), "realloc errors keep the block unchanged");
    recorder.fail = true;
    error = -1;
    Require(ft_mem_realloc_nid_postfix(&recorder.memory, 4, 2, 6, shrunk, &error) == shrunk && error == 0x40 && Filled(shrunk, 8, 0x22), "realloc keeps the block when realloc fails");
    recorder.fail = false;
    ft_mem_free_nid_postfix(&recorder.memory, shrunk);
}

void Duplication() {
    Recorder recorder;
    Reset(recorder);
    const char source[7] = {'a', 'b', 'c', 0, 'd', 'e', 'f'};
    std::int32_t error = -1;
    void* copy = ft_mem_dup_nid_postfix(&recorder.memory, source, sizeof(source), &error);
    Require(copy && error == 0 && recorder.allocs == 1 && recorder.lastSize == 7, "dup allocates size bytes");
    Require(std::memcmp(copy, source, sizeof(source)) == 0, "dup copies every byte, including embedded zeros");
    ft_mem_free_nid_postfix(&recorder.memory, copy);

    error = -1;
    Require(!ft_mem_dup_nid_postfix(&recorder.memory, source, 0, &error) && error == 0 && recorder.allocs == 1, "dup of size 0 returns null");
    error = -1;
    void* uncopied = ft_mem_dup_nid_postfix(&recorder.memory, nullptr, 4, &error);
    Require(uncopied && error == 0 && recorder.allocs == 2 && Filled(uncopied, 4, 0xAA), "dup of a null address allocates without copying");
    ft_mem_free_nid_postfix(&recorder.memory, uncopied);
    error = -1;
    Require(!ft_mem_dup_nid_postfix(&recorder.memory, source, 0x8000000000000000ull, &error) && error == 0x06 && recorder.allocs == 2, "dup of a size above FT_Long is Invalid_Argument");

    error = -1;
    char* text = static_cast<char*>(ft_mem_strdup_nid_postfix(&recorder.memory, "hello", &error));
    Require(text && error == 0 && recorder.lastSize == 6 && std::strcmp(text, "hello") == 0, "strdup copies strlen + 1 bytes");
    ft_mem_free_nid_postfix(&recorder.memory, text);
    const int allocs = recorder.allocs;
    error = -1;
    Require(!ft_mem_strdup_nid_postfix(&recorder.memory, nullptr, &error) && error == 0 && recorder.allocs == allocs, "strdup of null returns null without allocating");
    recorder.fail = true;
    error = -1;
    Require(!ft_mem_strdup_nid_postfix(&recorder.memory, "x", &error) && error == 0x40, "strdup reports Out_Of_Memory when alloc fails");
}

void BoundedCopy() {
    char buffer[8];
    std::memset(buffer, 'x', sizeof(buffer));
    Require(ft_mem_strcpyn_nid_postfix(buffer, "abc", sizeof(buffer)) == 0 && std::strcmp(buffer, "abc") == 0, "strcpyn copies a string that fits and returns 0");
    std::memset(buffer, 'x', sizeof(buffer));
    Require(ft_mem_strcpyn_nid_postfix(buffer, "abc", 4) == 0 && std::strcmp(buffer, "abc") == 0, "strcpyn fits size - 1 characters");
    std::memset(buffer, 'x', sizeof(buffer));
    Require(ft_mem_strcpyn_nid_postfix(buffer, "abcdef", 4) == 1 && std::strcmp(buffer, "abc") == 0 && buffer[4] == 'x', "strcpyn truncates to size - 1 characters and returns 1");
    std::memset(buffer, 'x', sizeof(buffer));
    Require(ft_mem_strcpyn_nid_postfix(buffer, "a", 1) == 1 && buffer[0] == 0 && buffer[1] == 'x', "strcpyn of size 1 writes only the terminator");
    std::memset(buffer, 'x', sizeof(buffer));
    Require(ft_mem_strcpyn_nid_postfix(buffer, "a", 0) == 1 && buffer[0] == 0 && buffer[1] == 'x', "strcpyn of size 0 still writes the terminator");
    std::memset(buffer, 'x', sizeof(buffer));
    Require(ft_mem_strcpyn_nid_postfix(buffer, "", 5) == 0 && buffer[0] == 0 && buffer[1] == 'x', "strcpyn of an empty string returns 0");
}

template<class F>
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

void LargeBlocks() {
    Recorder recorder;
    Reset(recorder);
    constexpr std::size_t size = 10000;
    std::int32_t error = -1;
    void* block = ft_mem_alloc_nid_postfix(&recorder.memory, size, &error);
    Require(block && error == 0 && Filled(block, size, 0), "alloc clears a block of several pages");
    error = -1;
    block = ft_mem_realloc_nid_postfix(&recorder.memory, 1, size, 3 * size, block, &error);
    Require(block && error == 0 && Filled(block, 3 * size, 0), "realloc clears a grown tail of several pages");
    auto* source = static_cast<unsigned char*>(std::malloc(size));
    for (std::size_t i = 0; i < size; ++i) source[i] = static_cast<unsigned char>(i * 7);
    error = -1;
    void* copy = ft_mem_dup_nid_postfix(&recorder.memory, source, size, &error);
    Require(copy && error == 0 && std::memcmp(copy, source, size) == 0, "dup copies a block of several pages");
    ft_mem_free_nid_postfix(&recorder.memory, copy);
    ft_mem_free_nid_postfix(&recorder.memory, block);
    std::free(source);
}

void GuestPointers() {
    const std::size_t pageSize = PageSize();
    char* pages = MapPages(3 * pageSize);
    char* readOnly = pages + pageSize;
    char* guard = pages + 2 * pageSize;
    std::memset(readOnly, 0x5A, pageSize);
    char* tail = guard - 4;
    std::memcpy(tail, "end", 4);
    Protect(readOnly, pageSize, true);
    Protect(guard, pageSize, false);
    auto* readOnlyError = reinterpret_cast<std::int32_t*>(readOnly);
    auto* unmappedError = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    auto* unmappedMemory = reinterpret_cast<GuestMemoryRec*>(std::uintptr_t{0x10});
    const auto* unmappedText = reinterpret_cast<const char*>(std::uintptr_t{0x10});

    Recorder recorder;
    Reset(recorder);
    std::int32_t error = -1;
    Require(RejectsKeepingErrno([&] { ft_mem_qalloc_nid_postfix(&recorder.memory, 8, unmappedError); }), "qalloc refuses an unmapped error");
    Require(RejectsKeepingErrno([&] { ft_mem_alloc_nid_postfix(&recorder.memory, 8, readOnlyError); }), "alloc refuses a read-only error");
    Require(RejectsKeepingErrno([&] { ft_mem_qrealloc_nid_postfix(&recorder.memory, 4, 0, 2, nullptr, unmappedError); }), "qrealloc refuses an unmapped error");
    Require(RejectsKeepingErrno([&] { ft_mem_realloc_nid_postfix(&recorder.memory, 4, 0, 2, nullptr, readOnlyError); }), "realloc refuses a read-only error");
    Require(RejectsKeepingErrno([&] { ft_mem_dup_nid_postfix(&recorder.memory, "ab", 3, unmappedError); }), "dup refuses an unmapped error");
    Require(RejectsKeepingErrno([&] { ft_mem_strdup_nid_postfix(&recorder.memory, "ab", readOnlyError); }), "strdup refuses a read-only error");
    Require(RejectsKeepingErrno([&] { ft_mem_qalloc_nid_postfix(&recorder.memory, 8, nullptr); }), "qalloc refuses a null error");
    Require(RejectsKeepingErrno([&] { ft_mem_alloc_nid_postfix(&recorder.memory, 8, nullptr); }), "alloc refuses a null error");
    Require(RejectsKeepingErrno([&] { ft_mem_qrealloc_nid_postfix(&recorder.memory, 4, 0, 2, nullptr, nullptr); }), "qrealloc refuses a null error");
    Require(RejectsKeepingErrno([&] { ft_mem_realloc_nid_postfix(&recorder.memory, 4, 0, 2, nullptr, nullptr); }), "realloc refuses a null error");
    Require(RejectsKeepingErrno([&] { ft_mem_dup_nid_postfix(&recorder.memory, "ab", 3, nullptr); }), "dup refuses a null error");
    Require(RejectsKeepingErrno([&] { ft_mem_strdup_nid_postfix(&recorder.memory, "ab", nullptr); }), "strdup refuses a null error");
    Require(recorder.allocs == 0 && recorder.reallocs == 0 && *readOnlyError == 0x5A5A5A5A, "an unwritable error is refused before any allocation");

    Require(RejectsKeepingErrno([&] { ft_mem_qalloc_nid_postfix(unmappedMemory, 8, &error); }), "qalloc refuses an unmapped memory record");
    Require(RejectsKeepingErrno([&] { ft_mem_alloc_nid_postfix(unmappedMemory, 8, &error); }), "alloc refuses an unmapped memory record");
    Require(RejectsKeepingErrno([&] { ft_mem_free_nid_postfix(unmappedMemory, tail); }), "free refuses an unmapped memory record");
    Require(RejectsKeepingErrno([&] { ft_mem_qrealloc_nid_postfix(unmappedMemory, 4, 1, 2, tail, &error); }), "qrealloc refuses an unmapped memory record");
    Require(error == -1, "an unmapped memory record leaves the error untouched");
    Require(!ft_mem_qalloc_nid_postfix(unmappedMemory, 0, &error) && error == 0, "qalloc of size 0 does not read the memory record");

    error = -1;
    Require(RejectsKeepingErrno([&] { ft_mem_strdup_nid_postfix(&recorder.memory, unmappedText, &error); }), "strdup refuses an unmapped string");
    Require(recorder.allocs == 0 && error == -1, "an unmapped string allocates nothing");
    char* text = static_cast<char*>(ft_mem_strdup_nid_postfix(&recorder.memory, tail, &error));
    Require(text && error == 0 && recorder.lastSize == 4 && std::strcmp(text, "end") == 0, "strdup copies a string that ends at a page end");
    ft_mem_free_nid_postfix(&recorder.memory, text);
    char* uncopied = nullptr;
    error = -1;
    const int allocsBeforeDup = recorder.allocs;
    const int freesBeforeDup = recorder.frees;
    Require(RejectsKeepingErrno([&] { uncopied = static_cast<char*>(ft_mem_dup_nid_postfix(&recorder.memory, unmappedText, 4, &error)); }),
            "dup refuses an unmapped source");
    Require(uncopied == nullptr && error == -1, "an unmapped source leaves the error untouched");
    Require(recorder.allocs == allocsBeforeDup + 1 && recorder.frees == freesBeforeDup + 1 && recorder.lastBlock != nullptr,
            "dup frees its block when the source is unmapped");

    char buffer[8];
    std::memset(buffer, 'x', sizeof(buffer));
    Require(RejectsKeepingErrno([&] { ft_mem_strcpyn_nid_postfix(buffer, unmappedText, sizeof(buffer)); }) && buffer[0] == 'x',
            "strcpyn refuses an unmapped source and writes nothing");
    Require(RejectsKeepingErrno([&] { ft_mem_strcpyn_nid_postfix(readOnly, "abc", 4); }) && readOnly[0] == 0x5A, "strcpyn refuses a read-only destination");
    Require(RejectsKeepingErrno([&] { ft_mem_strcpyn_nid_postfix(reinterpret_cast<char*>(std::uintptr_t{0x10}), "abc", 4); }),
            "strcpyn refuses an unmapped destination");
    Require(ft_mem_strcpyn_nid_postfix(buffer, tail, 1u << 20) == 0 && std::strcmp(buffer, "end") == 0 && buffer[4] == 'x',
            "strcpyn reads a source that ends at a page end, whatever the size");
    Protect(readOnly, pageSize, false);
    char* writable = readOnly - 4;
    std::memcpy(writable, "ab", 3);
    char* last = writable + 1;
    Require(ft_mem_strcpyn_nid_postfix(last, "zzz", 3) == 1 && std::memcmp(writable, "azz", 3) == 0 && *(last + 2) == 0,
            "strcpyn writes a destination that ends at a page end");
    UnmapPages(pages, 3 * pageSize);
}

void InternalCallers() {
#ifdef __linux__
    Recorder recorder;
    Reset(recorder);
    std::int32_t error = -1;
    procmapQueries = 0;
    ft_mem_free_nid_postfix(&recorder.memory, ft_mem_alloc_nid_postfix(&recorder.memory, 8, &error));
    if (procmapQueries == 0) {
        std::puts("FreeType Ot memory: no PROCMAP_QUERY on this host, internal caller check skipped");
        return;
    }
    procmapQueries = 0;
    auto* tags = static_cast<unsigned char*>(FreeType::OtMemRealloc(&recorder.memory, 1, 0, 512, nullptr, &error));
    Require(tags && error == 0 && Filled(tags, 512, 0), "an internal caller gets a cleared array");
    FreeType::OtMemFree(&recorder.memory, tags);
    Require(recorder.frees == 2, "an internal caller frees its array");
    Require(procmapQueries == 0, "internal callers allocate and free without guest pointer checks");
    FreeType::GuestOutline outline{};
    Require(FT_Outline_New_Internal_nid_postfix(&recorder.memory, 512, 16, &outline) == 0, "an outline allocates its arrays");
    Require(outline.points && outline.tags && outline.contours && Filled(outline.tags, 512, 0), "an outline gets cleared arrays");
    Require(FT_Outline_Done_Internal_nid_postfix(&recorder.memory, &outline) == 0 && recorder.frees == 5, "an outline frees its arrays");
#endif
}

}

int main() {
    QuickAllocation();
    ClearedAllocation();
    QuickReallocation();
    ClearedReallocation();
    Duplication();
    BoundedCopy();
    LargeBlocks();
    GuestPointers();
    InternalCallers();
    std::puts("Guest FreeType Ot memory tests passed");
    return 0;
}
