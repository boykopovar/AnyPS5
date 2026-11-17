#include "prx/libSceFreeType/include/MemoryRecord.hpp"
#include "Testing.hpp"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

int main(int count, char** arguments) {
    Require(count == 3);
    const bool patched = std::strcmp(arguments[2], "patched") == 0;
    Require(patched || std::strcmp(arguments[2], "unpatched") == 0);
    const char* const newSymbol = patched ? "tCMhTh7h8D4" : "FT_New_Memory_nid_postfix";
    const char* const doneSymbol = patched ? "UTumVjeIQtg" : "FT_Done_Memory_nid_postfix";
#ifdef _WIN32
    const auto module = LoadLibraryW(std::filesystem::path(arguments[1]).c_str());
    Require(module != nullptr);
    const auto newAddress = GetProcAddress(module, newSymbol);
    const auto doneAddress = GetProcAddress(module, doneSymbol);
#else
    const auto module = dlopen(arguments[1], RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) std::fprintf(stderr, "%s\n", dlerror());
    Require(module != nullptr);
    const auto newAddress = dlsym(module, newSymbol);
    const auto doneAddress = dlsym(module, doneSymbol);
#endif
    if (newAddress == nullptr) std::fprintf(stderr, "Missing FreeType entry: %s\n", newSymbol);
    if (doneAddress == nullptr) std::fprintf(stderr, "Missing FreeType entry: %s\n", doneSymbol);
    Require(newAddress != nullptr);
    Require(doneAddress != nullptr);
    const auto create = std::bit_cast<decltype(&FT_New_Memory_nid_postfix)>(newAddress);
    const auto destroy = std::bit_cast<decltype(&FT_Done_Memory_nid_postfix)>(doneAddress);

    FreeType::GuestMemoryRec* memory = create();
    Require(memory != nullptr);
    Require(memory->user == nullptr);
    Require(memory->alloc != nullptr);
    Require(memory->free != nullptr);
    Require(memory->realloc != nullptr);

    unsigned char* block = static_cast<unsigned char*>(memory->alloc(memory, 64));
    Require(block != nullptr);
    for (int index = 0; index < 64; ++index) block[index] = static_cast<unsigned char>(index);
    block = static_cast<unsigned char*>(memory->realloc(memory, 64, 128, block));
    Require(block != nullptr);
    for (int index = 0; index < 64; ++index) Require(block[index] == static_cast<unsigned char>(index));
    memory->free(memory, block);

    destroy(memory);
    destroy(nullptr);

    for (int index = 0; index < 256; ++index) {
        FreeType::GuestMemoryRec* record = create();
        Require(record != nullptr);
        void* probe = record->alloc(record, 1024);
        Require(probe != nullptr);
        record->free(record, probe);
        destroy(record);
    }

#ifdef _WIN32
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(module) == 0);
#endif
}
