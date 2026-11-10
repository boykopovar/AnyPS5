#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Search = char* (APS5_VABI*)(const char*, int);

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

char* Expected(const char* text, int character) {
    const char wanted = static_cast<char>(character);
    for (;;) {
        if (*text == wanted) return const_cast<char*>(text);
        if (*text == '\0') return nullptr;
        ++text;
    }
}

void Check(Search search, const char* text) {
    for (int character = -512; character <= 512; ++character) {
        Require(search(text, character) == Expected(text, character), "index returned the wrong position");
    }
    for (int character : {INT_MIN, INT_MAX}) {
        Require(search(text, character) == Expected(text, character), "index truncated an int incorrectly");
    }
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "expected the library path");
#ifdef _WIN32
    const auto library = LoadLibraryA(argv[1]);
    Require(library != nullptr, "could not load libc");
    const auto search = reinterpret_cast<Search>(GetProcAddress(library, "index_nid_postfix"));
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    auto* memory = static_cast<char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr, "could not allocate the boundary fixture");
    DWORD previous = 0;
    Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0, "could not protect the boundary fixture");
#else
    const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    Require(library != nullptr, "could not load libc");
    const auto search = reinterpret_cast<Search>(dlsym(library, "index_nid_postfix"));
    const long pageSize = sysconf(_SC_PAGESIZE);
    Require(pageSize > 0, "could not query page size");
    const auto page = static_cast<std::size_t>(pageSize);
    auto* memory = static_cast<char*>(mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(memory != MAP_FAILED, "could not allocate the boundary fixture");
    Require(mprotect(memory + page, page, PROT_NONE) == 0, "could not protect the boundary fixture");
#endif
    Require(search != nullptr, "index is missing from libc");
    Check(search, "");
    Check(search, "banana");
    const std::array<char, 9> bytes{'a', static_cast<char>(0x80), 'a', static_cast<char>(0xff), '\0', 'z', 'a', '\0', '\0'};
    const auto before = bytes;
    Check(search, bytes.data());
    Require(bytes == before, "index modified its input");
    for (std::size_t length = 0; length <= 256; ++length) {
        char* text = memory + page - length - 1;
        for (std::size_t i = 0; i < length; ++i) text[i] = static_cast<char>(1 + i % 255);
        text[length] = '\0';
        Check(search, text);
    }
    char writable[] = "xyx";
    char* found = search(writable, 'y');
    Require(found == writable + 1, "index did not return the caller's storage");
    *found = 'z';
    Require(std::strcmp(writable, "xzx") == 0, "index returned an unwritable copy");
#ifdef _WIN32
    Require(VirtualFree(memory, 0, MEM_RELEASE) != 0, "could not free the boundary fixture");
    FreeLibrary(library);
#else
    Require(munmap(memory, 2 * page) == 0, "could not free the boundary fixture");
    dlclose(library);
#endif
    return 0;
}
