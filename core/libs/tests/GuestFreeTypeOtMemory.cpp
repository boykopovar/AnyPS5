#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

}

int main() {
    QuickAllocation();
    ClearedAllocation();
    QuickReallocation();
    ClearedReallocation();
    Duplication();
    BoundedCopy();
    std::puts("Guest FreeType Ot memory tests passed");
    return 0;
}
