#include <cstddef>
#include <cstdio>
#include <new>
#include <windows.h>

namespace {

[[gnu::noinline]] void* AllocateWithoutUnwindData(std::size_t size) {
    void* const block = ::operator new(size);
    std::printf("allocated %p\n", block);
    return block;
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <libc.prx>\n", argv[0]);
        return 2;
    }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (LoadLibraryExA(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) == nullptr) {
        std::fprintf(stderr, "%s: LoadLibraryEx failed with error %lu\n", argv[1], GetLastError());
        return 1;
    }
    AllocateWithoutUnwindData(std::size_t{1} << 46);
    std::fputs("operator new returned instead of throwing\n", stderr);
    return 1;
}
