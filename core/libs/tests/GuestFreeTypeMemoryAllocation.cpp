#include "prx/libSceFreeType/include/MemoryAllocation.hpp"
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

int g_frees = 0;
int g_allocations = 0;
bool g_failAllocation = false;

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
    return std::realloc(block, static_cast<std::size_t>(size));
}

}

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "FreeType legacy allocator check failed at line %d\n", line);
        std::exit(EXIT_FAILURE);
    }
}
#define Check(value) Require((value), __LINE__)

int main(int count, char** arguments) {
    Check(count == 3);
    const bool patched = std::strcmp(arguments[2], "patched") == 0;
    Check(patched || std::strcmp(arguments[2], "unpatched") == 0);
    const char* const allocSymbol = patched ? "Uw+BZJpflxk" : "FT_Alloc_nid_postfix";
    const char* const qAllocSymbol = patched ? "hz-J3hohXW8" : "FT_QAlloc_nid_postfix";
    const char* const freeSymbol = patched ? "fHMsRluEMfI" : "FT_Free_nid_postfix";
#ifdef _WIN32
    const auto module = LoadLibraryW(std::filesystem::path(arguments[1]).c_str());
    Check(module != nullptr);
    const auto allocAddress = GetProcAddress(module, allocSymbol);
    const auto qAllocAddress = GetProcAddress(module, qAllocSymbol);
    const auto freeAddress = GetProcAddress(module, freeSymbol);
#else
    const auto module = dlopen(arguments[1], RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) std::fprintf(stderr, "%s\n", dlerror());
    Check(module != nullptr);
    const auto allocAddress = dlsym(module, allocSymbol);
    const auto qAllocAddress = dlsym(module, qAllocSymbol);
    const auto freeAddress = dlsym(module, freeSymbol);
#endif
    Check(allocAddress != nullptr);
    Check(qAllocAddress != nullptr);
    Check(freeAddress != nullptr);
    const auto alloc = std::bit_cast<decltype(&FT_Alloc_nid_postfix)>(allocAddress);
    const auto qAlloc = std::bit_cast<decltype(&FT_QAlloc_nid_postfix)>(qAllocAddress);
    const auto release = std::bit_cast<decltype(&FT_Free_nid_postfix)>(freeAddress);

    FreeType::GuestMemoryRec memory{nullptr, TestAlloc, TestFree, TestRealloc};

    void* block = reinterpret_cast<void*>(0x12345678);
    Check(alloc(&memory, 64, &block) == 0);
    Check(block != nullptr);
    for (int index = 0; index < 64; ++index)
        Check(static_cast<unsigned char*>(block)[index] == 0);

    release(&memory, &block);
    Check(block == nullptr);
    Check(g_frees == 1);

    block = reinterpret_cast<void*>(0x12345678);
    Check(qAlloc(&memory, 48, &block) == 0);
    Check(block != nullptr);
    for (int index = 0; index < 48; ++index)
        Check(static_cast<unsigned char*>(block)[index] == 0xa5);
    release(&memory, &block);
    Check(block == nullptr);
    Check(g_frees == 2);

    release(&memory, &block);
    Check(g_frees == 2);

    Check(qAlloc(&memory, 16, &block) == 0 && block != nullptr);
    void* const retained = block;
    const int allocations = g_allocations;
    release(nullptr, &block);
    Check(block == retained && g_frees == 2);
    for (int index = 0; index < 16; ++index)
        Check(static_cast<unsigned char*>(block)[index] == 0xa5);
    release(&memory, nullptr);
    release(nullptr, nullptr);
    Check(block == retained && g_frees == 2 && g_allocations == allocations);
    release(&memory, &block);
    Check(block == nullptr && g_frees == 3);

    for (const auto allocate : {alloc, qAlloc}) {
        const int allocations = g_allocations;
        block = reinterpret_cast<void*>(0x12345678);
        Check(allocate(&memory, 0, &block) == 0 && block == nullptr);
        for (const auto size : {std::int64_t{-1}, std::int64_t{-8}, std::numeric_limits<std::int64_t>::min()}) {
            block = reinterpret_cast<void*>(0x12345678);
            Check(allocate(&memory, size, &block) == kInvalidArgument && block == nullptr);
        }
        Check(g_allocations == allocations);
    }
    Check(g_frees == 3);

    g_failAllocation = true;
    Check(alloc(&memory, 32, &block) == kOutOfMemory && block == nullptr);
    Check(qAlloc(&memory, 32, &block) == kOutOfMemory && block == nullptr);
    g_failAllocation = false;
    Check(g_frees == 3);

#ifdef _WIN32
    Check(FreeLibrary(module) != 0);
#else
    Check(dlclose(module) == 0);
#endif
}
