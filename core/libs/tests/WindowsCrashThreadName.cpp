#include <cstdint>
#include <cstdio>
#include <string>
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
    HANDLE worker = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        Sleep(INFINITE);
        return 0;
    }, nullptr, 0, nullptr);
    if (worker == nullptr) {
        std::fprintf(stderr, "CreateThread failed with error %lu\n", GetLastError());
        return 1;
    }
    const std::wstring mainName = std::wstring(126, L'a') + L'Z' + std::wstring(70, L'b');
    const std::wstring workerName = std::wstring(126, L'c') + L'Y' + std::wstring(70, L'd');
    const HRESULT mainResult = SetThreadDescription(GetCurrentThread(), mainName.c_str());
    const HRESULT workerResult = SetThreadDescription(worker, workerName.c_str());
    if (FAILED(mainResult) || FAILED(workerResult)) {
        std::fprintf(stderr, "SetThreadDescription failed: 0x%08lx 0x%08lx\n", static_cast<unsigned long>(mainResult), static_cast<unsigned long>(workerResult));
        return 1;
    }
    std::fputs("faulting\n", stderr);
    std::fflush(stderr);
    const std::uintptr_t unmapped = 8;
    asm volatile("movq (%0), %%rax" : : "r"(unmapped) : "rax", "memory");
    std::fputs("the read of address 8 did not fault\n", stderr);
    return 1;
}
