#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

int main(int argc, char** argv) {
    if (argc != 2) return 2;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    if (std::strcmp(argv[1], "buffered") == 0) {
        static char buffer[4096];
        if (std::setvbuf(stderr, buffer, _IOFBF, sizeof(buffer)) != 0) return 4;
    } else if (std::strcmp(argv[1], "plain") != 0) {
        return 2;
    }
    _ZNSt3__122__libcpp_verbose_abortEPKcz_nid_postfix("libc++ diagnostic: %d %s\n", 37, "sentinel");
}
