#include "prx/libc/include/Api.hpp"
#include <cstdint>
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
    if (std::strcmp(argv[1], "abort") == 0) {
        abort_nid_postfix(0, 0, 0, 0, 0, 0);
    } else if (std::strcmp(argv[1], "stack") == 0) {
        __stack_chk_fail_nid_postfix();
    } else {
        return 2;
    }
}
