#include "prx/libSceFreeType/include/MemoryRecord.hpp"
#include "prx/libSceFreeTypeOt/include/Outline.hpp"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using FreeType::GuestMemoryRec;
using FreeType::GuestOutline;
using FreeType::GuestVector;

struct Api {
    decltype(&FT_Outline_New_Internal_nid_postfix) newOutline;
    decltype(&FT_Outline_Done_Internal_nid_postfix) doneOutline;
    decltype(&FT_New_Memory_nid_postfix) newMemory;
    decltype(&FT_Done_Memory_nid_postfix) doneMemory;
};

struct Recorder {
    GuestMemoryRec memory{};
    std::vector<std::int64_t> sizes;
    std::vector<void*> blocks;
    std::vector<void*> freed;
    int failAt = -1;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot outline: %s\n", message);
    std::exit(1);
}

Recorder* Owner(GuestMemoryRec* memory) {
    return static_cast<Recorder*>(memory->user);
}

void* APS5_VABI RecordAlloc(GuestMemoryRec* memory, std::int64_t size) {
    auto* recorder = Owner(memory);
    recorder->sizes.push_back(size);
    if (recorder->failAt == static_cast<int>(recorder->sizes.size())) return nullptr;
    void* block = std::malloc(static_cast<std::size_t>(size));
    std::memset(block, 0xAA, static_cast<std::size_t>(size));
    recorder->blocks.push_back(block);
    return block;
}

void APS5_VABI RecordFree(GuestMemoryRec* memory, void* block) {
    Owner(memory)->freed.push_back(block);
    std::free(block);
}

void* APS5_VABI RecordRealloc(GuestMemoryRec*, std::int64_t, std::int64_t, void*) {
    Require(false, "an outline allocation never reallocates");
    return nullptr;
}

void Reset(Recorder& recorder) {
    recorder = Recorder{};
    recorder.memory = {&recorder, RecordAlloc, RecordFree, RecordRealloc};
}

bool Zero(const void* block, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(block);
    for (std::size_t i = 0; i < size; ++i) {
        if (bytes[i] != 0) return false;
    }
    return true;
}

bool Empty(const GuestOutline& outline) {
    return outline.n_contours == 0 && outline.n_points == 0 && !outline.points && !outline.tags && !outline.contours && outline.flags == 0;
}

GuestOutline Sentinel() {
    GuestOutline outline{};
    std::memset(&outline, 0x5C, sizeof(outline));
    return outline;
}

bool Same(const GuestOutline& left, const GuestOutline& right) {
    return std::memcmp(&left, &right, sizeof(GuestOutline)) == 0;
}

void NullArguments(const Api& api) {
    Recorder recorder;
    Reset(recorder);
    GuestOutline outline = Sentinel();
    Require(api.newOutline(nullptr, 1, 1, &outline) == 0x06 && Same(outline, Sentinel()), "a null memory is Invalid_Argument and leaves the outline");
    Require(api.newOutline(&recorder.memory, 1, 1, nullptr) == 0x06 && recorder.sizes.empty(), "a null outline is Invalid_Argument without allocating");
    Require(api.doneOutline(nullptr, &outline) == 0x06 && Same(outline, Sentinel()), "done with a null memory is Invalid_Argument and leaves the outline");
    Require(api.doneOutline(&recorder.memory, nullptr) == 0x06 && recorder.freed.empty(), "done with a null outline is Invalid_Argument");
}

void Allocation(const Api& api) {
    Recorder recorder;
    Reset(recorder);
    GuestOutline outline = Sentinel();
    Require(api.newOutline(&recorder.memory, 5, 2, &outline) == 0, "a small outline is allocated");
    Require(recorder.sizes == std::vector<std::int64_t>{80, 5, 4}, "points, tags and contours are allocated in that order at 16, 1 and 2 bytes per item");
    Require(outline.points == recorder.blocks[0] && outline.tags == recorder.blocks[1] && outline.contours == recorder.blocks[2], "the arrays are the allocated blocks");
    Require(Zero(outline.points, 80) && Zero(outline.tags, 5) && Zero(outline.contours, 4), "the arrays are cleared");
    Require(outline.n_points == 5 && outline.n_contours == 2 && outline.flags == 1, "the counts are stored and only the owner flag is set");
    const std::vector<void*> blocks = recorder.blocks;
    Require(api.doneOutline(&recorder.memory, &outline) == 0, "done of an owned outline succeeds");
    Require(recorder.freed == blocks && Empty(outline), "done frees points, tags and contours in that order and clears the outline");

    Reset(recorder);
    outline = Sentinel();
    Require(api.newOutline(&recorder.memory, 0, 0, &outline) == 0 && recorder.sizes.empty(), "an empty outline allocates nothing");
    Require(outline.n_points == 0 && outline.n_contours == 0 && !outline.points && !outline.tags && !outline.contours && outline.flags == 1, "an empty outline is owned and has null arrays");
    Require(api.doneOutline(&recorder.memory, &outline) == 0 && recorder.freed.empty() && Empty(outline), "done of an empty outline frees nothing");

    Reset(recorder);
    Require(api.newOutline(&recorder.memory, 70000, 0x12345, &outline) == 0, "counts above 16 bits are allocated in full");
    Require(recorder.sizes == std::vector<std::int64_t>{70000 * 16, 70000, 0x12345 * 2}, "the full counts size the arrays");
    Require(outline.n_points == (70000 & 0xFFFF) && outline.n_contours == 0x2345, "the stored counts keep the low 16 bits");
    Require(api.doneOutline(&recorder.memory, &outline) == 0 && recorder.freed.size() == 3, "done frees the large arrays");

    Reset(recorder);
    Require(api.newOutline(&recorder.memory, 40000, 1, &outline) == 0 && outline.n_points == 40000, "a count of 0x8000 or more keeps its 16-bit pattern");
    Require(api.doneOutline(&recorder.memory, &outline) == 0, "done of the 40000-point outline succeeds");
}

void Failures(const Api& api) {
    Recorder recorder;
    Reset(recorder);
    GuestOutline outline = Sentinel();
    Require(api.newOutline(&recorder.memory, 3, -1, &outline) == 0x06, "a negative contour count is Invalid_Argument");
    Require(recorder.sizes == std::vector<std::int64_t>{48, 3} && recorder.freed == recorder.blocks && Empty(outline), "the points and tags already allocated are freed and the outline is cleared");

    Reset(recorder);
    outline = Sentinel();
    Require(api.newOutline(&recorder.memory, 0x08000000, 1, &outline) == 0x0A, "a point array above FT_INT_MAX bytes is Array_Too_Large");
    Require(recorder.sizes.empty() && recorder.freed.empty() && Empty(outline), "nothing is allocated for an oversized point array");

    Reset(recorder);
    Require(api.newOutline(&recorder.memory, 0xFFFFFFFFu, 1, &outline) == 0x0A && recorder.sizes.empty() && Empty(outline), "the point count is unsigned");

    Reset(recorder);
    recorder.failAt = 1;
    Require(api.newOutline(&recorder.memory, 0x07FFFFFF, 0, &outline) == 0x40, "the largest point count that fits reaches the allocator");
    Require(recorder.sizes == std::vector<std::int64_t>{0x7FFFFFF0} && recorder.freed.empty() && Empty(outline), "the largest point array is 0x7FFFFFF0 bytes");

    Reset(recorder);
    outline = Sentinel();
    Require(api.newOutline(&recorder.memory, 1, 0x40000000, &outline) == 0x0A, "a contour array above FT_INT_MAX bytes is Array_Too_Large");
    Require(recorder.sizes == std::vector<std::int64_t>{16, 1} && recorder.freed == recorder.blocks && Empty(outline), "the points and tags are freed after an oversized contour array");

    Reset(recorder);
    recorder.failAt = 2;
    outline = Sentinel();
    Require(api.newOutline(&recorder.memory, 4, 1, &outline) == 0x40, "a failed tag allocation is Out_Of_Memory");
    Require(recorder.sizes == std::vector<std::int64_t>{64, 4} && recorder.freed == recorder.blocks && Empty(outline), "the points are freed after a failed tag allocation");

    Reset(recorder);
    recorder.failAt = 3;
    Require(api.newOutline(&recorder.memory, 4, 1, &outline) == 0x40 && recorder.freed == recorder.blocks && recorder.freed.size() == 2 && Empty(outline), "a failed contour allocation frees points and tags");
}

void Ownership(const Api& api) {
    Recorder recorder;
    Reset(recorder);
    GuestVector points[2]{};
    unsigned char tags[2]{};
    std::uint16_t contours[1]{};
    GuestOutline outline{1, 2, points, tags, contours, 0x104};
    Require(api.doneOutline(&recorder.memory, &outline) == 0 && recorder.freed.empty() && Empty(outline), "done of a borrowed outline frees nothing and clears it");

    void* ownedPoints = std::malloc(16);
    void* ownedTags = std::malloc(1);
    outline = {0, 1, static_cast<GuestVector*>(ownedPoints), static_cast<unsigned char*>(ownedTags), nullptr, 0x105};
    Require(api.doneOutline(&recorder.memory, &outline) == 0, "done of an owned outline with other flags succeeds");
    Require(recorder.freed == std::vector<void*>{ownedPoints, ownedTags} && Empty(outline), "the owner bit alone decides the frees and null arrays are skipped");
}

void HostMemory(const Api& api) {
    GuestMemoryRec* memory = api.newMemory();
    Require(memory && memory->user == nullptr && memory->alloc && memory->free && memory->realloc, "FT_New_Memory returns a record with null user data and the C allocator");
    GuestOutline outline = Sentinel();
    Require(api.newOutline(memory, 3, 1, &outline) == 0 && outline.points && outline.tags && outline.contours, "an outline allocates through the FT_New_Memory record");
    Require(Zero(outline.points, 48) && Zero(outline.tags, 3) && Zero(outline.contours, 2), "the record's arrays are cleared");
    Require(api.doneOutline(memory, &outline) == 0 && Empty(outline), "the record frees the outline");
    api.doneMemory(memory);
}

template <typename T>
void Resolve(T& target, void* module, const char* symbol) {
#ifdef _WIN32
    target = std::bit_cast<T>(GetProcAddress(static_cast<HMODULE>(module), symbol));
#else
    target = std::bit_cast<T>(dlsym(module, symbol));
#endif
    if (!target) std::fprintf(stderr, "FreeType Ot outline: missing %s\n", symbol);
    Require(target != nullptr, "every entry resolves");
}

void* Open(const char* path) {
#ifdef _WIN32
    void* module = LoadLibraryW(std::filesystem::path(path).c_str());
#else
    void* module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr, "the module loads");
    return module;
}

}

int main(int count, char** arguments) {
    Require(count == 3, "usage: <libSceFreeTypeOt> raw|patched");
    const bool patched = std::strcmp(arguments[2], "patched") == 0;
    Require(patched || std::strcmp(arguments[2], "raw") == 0, "the mode is raw or patched");
    void* ot = Open(arguments[1]);
    Api api{};
    Resolve(api.newOutline, ot, patched ? "NG7OgYsgcF8" : "FT_Outline_New_Internal_nid_postfix");
    Resolve(api.doneOutline, ot, patched ? "umIqupy8UU0" : "FT_Outline_Done_Internal_nid_postfix");
    Resolve(api.newMemory, ot, patched ? "tCMhTh7h8D4" : "FT_New_Memory_nid_postfix");
    Resolve(api.doneMemory, ot, patched ? "UTumVjeIQtg" : "FT_Done_Memory_nid_postfix");
    NullArguments(api);
    Allocation(api);
    Failures(api);
    Ownership(api);
    HostMemory(api);
    std::puts("FreeType Ot outline: all checks passed");
    return 0;
}
