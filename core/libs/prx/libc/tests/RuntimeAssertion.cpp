#include "prx/libc/include/exceptions/Runtime.hpp"
#include <cstdlib>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

int main(int argc, char** argv) {
    if (argc != 5 && argc != 6) return 2;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    static char errorBuffer[4096];
    if (argc == 6 && std::setvbuf(stderr, errorBuffer, _IOFBF, sizeof(errorBuffer)) != 0) return 4;
    LibcRuntimeAssertion(argv[1], static_cast<int>(std::strtol(argv[2], nullptr, 10)), argv[3], argv[4]);
}
