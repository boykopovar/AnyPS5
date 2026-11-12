#include <climits>
#include <cstdio>
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
    volatile int dividend = INT_MIN;
    volatile int divisor = -1;
    int quotient = dividend;
    const int by = divisor;
    std::fputs("dividing\n", stderr);
    std::fflush(stderr);
    asm volatile(
        "cltd\n\t"
        "idivl %1"
        : "+a"(quotient)
        : "r"(by)
        : "rdx", "cc");
    std::fprintf(stderr, "the division did not fault: %d\n", quotient);
    return 1;
}
