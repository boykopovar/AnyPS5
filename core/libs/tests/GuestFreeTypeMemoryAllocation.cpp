#include "prx/libSceFreeType/include/MemoryAllocation.hpp"
#include "Testing.hpp"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int kOutOfMemory = 64;
constexpr int kInvalidArgument = 6;
constexpr int kArrayTooLarge = 10;

int g_frees = 0;
int g_allocations = 0;
int g_reallocations = 0;
bool g_failAllocation = false;
bool g_failReallocation = false;

void* APS5_VABI TestAlloc(FreeType::GuestMemoryRec* memory, std::int64_t size) {
    ++g_allocations;
    if (g_failAllocation) return nullptr;
    void* const block = std::malloc(static_cast<std::size_t>(size));
    if (block) std::memset(block, 0xa5, static_cast<std::size_t>(size));
    return block;
}

void APS5_VABI TestFree(FreeType::GuestMemoryRec* memory, void* block) {
    ++g_frees;
    std::free(block);
}

void* APS5_VABI TestRealloc(FreeType::GuestMemoryRec* memory, std::int64_t current, std::int64_t size, void* block) {
    ++g_reallocations;
    if (g_failReallocation) return nullptr;
    void* const grown = std::realloc(block, static_cast<std::size_t>(size));
    if (grown && size > current)
        std::memset(static_cast<unsigned char*>(grown) + current, 0xa5, static_cast<std::size_t>(size - current));
    return grown;
}

}

int main(int count, char** arguments) {
    Require(count == 3);
    const bool patched = std::strcmp(arguments[2], "patched") == 0;
    Require(patched || std::strcmp(arguments[2], "unpatched") == 0);
    const char* const allocSymbol = patched ? "Uw+BZJpflxk" : "FT_Alloc_nid_postfix";
    const char* const qAllocSymbol = patched ? "hz-J3hohXW8" : "FT_QAlloc_nid_postfix";
    const char* const reallocSymbol = patched ? "HVSaZbxYi2s" : "FT_Realloc_nid_postfix";
    const char* const qReallocSymbol = patched ? "KVbmKCGGowg" : "FT_QRealloc_nid_postfix";
    const char* const freeSymbol = patched ? "fHMsRluEMfI" : "FT_Free_nid_postfix";
#ifdef _WIN32
    const auto module = LoadLibraryW(std::filesystem::path(arguments[1]).c_str());
    Require(module != nullptr);
    const auto allocAddress = GetProcAddress(module, allocSymbol);
    const auto qAllocAddress = GetProcAddress(module, qAllocSymbol);
    const auto reallocAddress = GetProcAddress(module, reallocSymbol);
    const auto qReallocAddress = GetProcAddress(module, qReallocSymbol);
    const auto freeAddress = GetProcAddress(module, freeSymbol);
#else
    const auto module = dlopen(arguments[1], RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) std::fprintf(stderr, "%s\n", dlerror());
    Require(module != nullptr);
    const auto allocAddress = dlsym(module, allocSymbol);
    const auto qAllocAddress = dlsym(module, qAllocSymbol);
    const auto reallocAddress = dlsym(module, reallocSymbol);
    const auto qReallocAddress = dlsym(module, qReallocSymbol);
    const auto freeAddress = dlsym(module, freeSymbol);
#endif
    Require(allocAddress != nullptr);
    Require(qAllocAddress != nullptr);
    Require(reallocAddress != nullptr);
    Require(qReallocAddress != nullptr);
    Require(freeAddress != nullptr);
    const auto alloc = std::bit_cast<decltype(&FT_Alloc_nid_postfix)>(allocAddress);
    const auto qAlloc = std::bit_cast<decltype(&FT_QAlloc_nid_postfix)>(qAllocAddress);
    const auto resize = std::bit_cast<decltype(&FT_Realloc_nid_postfix)>(reallocAddress);
    const auto qResize = std::bit_cast<decltype(&FT_QRealloc_nid_postfix)>(qReallocAddress);
    const auto release = std::bit_cast<decltype(&FT_Free_nid_postfix)>(freeAddress);

    FreeType::GuestMemoryRec memory{nullptr, TestAlloc, TestFree, TestRealloc};

    void* block = reinterpret_cast<void*>(0x12345678);
    Require(alloc(&memory, 64, &block) == 0);
    Require(block != nullptr);
    for (int index = 0; index < 64; ++index)
        Require(static_cast<unsigned char*>(block)[index] == 0);

    release(&memory, &block);
    Require(block == nullptr);
    Require(g_frees == 1);

    block = reinterpret_cast<void*>(0x12345678);
    Require(qAlloc(&memory, 48, &block) == 0);
    Require(block != nullptr);
    for (int index = 0; index < 48; ++index)
        Require(static_cast<unsigned char*>(block)[index] == 0xa5);
    release(&memory, &block);
    Require(block == nullptr);
    Require(g_frees == 2);

    release(&memory, &block);
    Require(g_frees == 2);

    Require(qAlloc(&memory, 16, &block) == 0 && block != nullptr);
    void* const retained = block;
    const int allocations = g_allocations;
    release(nullptr, &block);
    Require(block == retained && g_frees == 2);
    for (int index = 0; index < 16; ++index)
        Require(static_cast<unsigned char*>(block)[index] == 0xa5);
    release(&memory, nullptr);
    release(nullptr, nullptr);
    Require(block == retained && g_frees == 2 && g_allocations == allocations);
    release(&memory, &block);
    Require(block == nullptr && g_frees == 3);

    for (const auto allocate : {alloc, qAlloc}) {
        const int allocations = g_allocations;
        block = reinterpret_cast<void*>(0x12345678);
        Require(allocate(&memory, 0, &block) == 0 && block == nullptr);
        for (const auto size : {std::int64_t{-1}, std::int64_t{-8}, std::numeric_limits<std::int64_t>::min()}) {
            block = reinterpret_cast<void*>(0x12345678);
            Require(allocate(&memory, size, &block) == kInvalidArgument && block == nullptr);
        }
        Require(g_allocations == allocations);
    }
    Require(g_frees == 3);

    g_failAllocation = true;
    Require(alloc(&memory, 32, &block) == kOutOfMemory && block == nullptr);
    Require(qAlloc(&memory, 32, &block) == kOutOfMemory && block == nullptr);
    g_failAllocation = false;
    Require(g_frees == 3);

    for (const auto reallocate : {resize, qResize}) {
        const bool zeroing = reallocate == resize;
        block = reinterpret_cast<void*>(0x12345678);
        Require(reallocate(&memory, 0, 64, &block) == 0);
        Require(block != nullptr);
        for (int index = 0; index < 64; ++index)
            Require(static_cast<unsigned char*>(block)[index] == 0);

        for (int index = 0; index < 64; ++index)
            static_cast<unsigned char*>(block)[index] = static_cast<unsigned char>(index * 7 + 1);
        Require(reallocate(&memory, 64, 128, &block) == 0);
        Require(block != nullptr);
        for (int index = 0; index < 64; ++index)
            Require(static_cast<unsigned char*>(block)[index] == static_cast<unsigned char>(index * 7 + 1));
        for (int index = 64; index < 128; ++index)
            Require(static_cast<unsigned char*>(block)[index] == (zeroing ? 0 : 0xa5));
        release(&memory, &block);
        Require(block == nullptr);
    }

    {
        void* const sentinel = reinterpret_cast<void*>(0x12345678);
        const int allocations = g_allocations;
        const int reallocations = g_reallocations;
        const int frees = g_frees;
        for (const auto reallocate : {resize, qResize}) {
            block = sentinel;
            Require(reallocate(&memory, -1, 64, &block) == kInvalidArgument && block == sentinel);
            block = sentinel;
            Require(reallocate(&memory, 64, -1, &block) == kInvalidArgument && block == sentinel);
            block = sentinel;
            Require(reallocate(&memory, std::numeric_limits<std::int64_t>::min(), 64, &block) == kInvalidArgument && block == sentinel);
            block = sentinel;
            Require(reallocate(&memory, 64, 2147483648ll, &block) == kArrayTooLarge && block == sentinel);
        }
        Require(g_allocations == allocations && g_reallocations == reallocations && g_frees == frees);
    }

    for (const auto reallocate : {resize, qResize}) {
        block = reinterpret_cast<void*>(0x12345678);
        Require(qAlloc(&memory, 32, &block) == 0 && block != nullptr);
        const int frees = g_frees;
        Require(reallocate(&memory, 32, 0, &block) == 0 && block == nullptr);
        Require(g_frees == frees + 1);
        block = nullptr;
        Require(reallocate(&memory, 0, 0, &block) == 0 && block == nullptr);
        Require(g_frees == frees + 1);
    }

    g_failReallocation = true;
    block = reinterpret_cast<void*>(0x12345678);
    Require(qAlloc(&memory, 64, &block) == 0 && block != nullptr);
    {
        void* const original = block;
        const int frees = g_frees;
        const int reallocations = g_reallocations;
        Require(resize(&memory, 64, 128, &block) == kOutOfMemory && block == original);
        Require(qResize(&memory, 64, 128, &block) == kOutOfMemory && block == original);
        Require(g_reallocations == reallocations + 2);
        Require(g_frees == frees);
        release(&memory, &block);
        Require(block == nullptr);
        Require(g_frees == frees + 1);
    }
    g_failReallocation = false;

    g_failAllocation = true;
    block = reinterpret_cast<void*>(0x12345678);
    Require(resize(&memory, 0, 32, &block) == kOutOfMemory && block == nullptr);
    Require(qResize(&memory, 0, 32, &block) == kOutOfMemory && block == nullptr);
    g_failAllocation = false;

#ifdef _WIN32
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(module) == 0);
#endif
}
