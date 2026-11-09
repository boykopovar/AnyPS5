#include <cstdint>
#include <cstdio>
#include <cstring>
#include <windows.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <libc.prx>\n", argv[0]);
        return 2;
    }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS*) -> LONG {
        TerminateProcess(GetCurrentProcess(), 3);
        return EXCEPTION_EXECUTE_HANDLER;
    });
    if (LoadLibraryExA(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) == nullptr) {
        std::fprintf(stderr, "%s: LoadLibraryEx failed with error %lu\n", argv[1], GetLastError());
        return 1;
    }
    static const char narrow[] = "allocation failed in the heap";
    static const char16_t wide[] = u"LowLevelFatalError [File:Unknown] [Line: 42] Out of memory";
    static const std::uint8_t binary[] = {0x08, 0xd4, 0x8e, 0x5b, 0xf6, 0x7f, 0x00, 0x00};
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD previous = 0;
    if (pages == nullptr || !VirtualProtect(pages + 0x1000, 0x1000, PAGE_NOACCESS, &previous)) {
        std::fprintf(stderr, "cannot prepare the page edge: error %lu\n", GetLastError());
        return 1;
    }
    char* const edge = pages + 0x1000 - 8;
    std::memcpy(edge, "PAGE END", 8);
    std::fputs("faulting\n", stderr);
    std::fflush(stderr);
    const std::uintptr_t unmapped = 8;
    asm volatile(
        "movq %0, %%r12\n\t"
        "movq %1, %%r13\n\t"
        "movq %2, %%r14\n\t"
        "movq %3, %%rbx\n\t"
        "movq (%4), %%rax"
        :
        : "r"(narrow), "r"(wide), "r"(edge), "r"(binary), "r"(unmapped)
        : "rax", "rbx", "r12", "r13", "r14", "memory");
    std::fputs("the read of address 8 did not fault\n", stderr);
    return 1;
}
