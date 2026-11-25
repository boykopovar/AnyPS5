#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdlib>
#endif

int main(int argc, char** argv) {
    if (argc != 2) return 2;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const auto* unmapped = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    if (std::strcmp(argv[1], "readable") == 0) _Assert_nid_postfix("count > 0", "Source.cpp:12");
    if (std::strcmp(argv[1], "unmapped") == 0) _Assert_nid_postfix(unmapped, unmapped);
    if (std::strcmp(argv[1], "null") == 0) _Assert_nid_postfix(nullptr, nullptr);
    return 3;
}
