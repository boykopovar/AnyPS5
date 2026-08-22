#include <cstdint>
#include <cstdlib>
#include <cerrno>
#include "SceTypes.hpp"

std::uintptr_t __stack_chk_guard = 0xDEADBEEFCAFEBABEull;

extern "C" {

void exit_nid_postfix(int code) {
    std::exit(code);
}

[[noreturn]] void abort_nid_postfix(
    uint64_t arg0, uint64_t arg1, uint64_t arg2,
    uint64_t arg3, uint64_t arg4, uint64_t arg5
) {
    (void)arg0; (void)arg1; (void)arg2;
    (void)arg3; (void)arg4; (void)arg5;
    std::abort();
}

int* libc_error_nid_postfix() {
    return &errno;
}

int* __error_nid_postfix() {
    return &errno;
}

[[noreturn]] void __stack_chk_fail_nid_postfix() {
    std::abort();
}

int atexit_nid_postfix(atexit_func_t func) {
    return std::atexit(func);
}

int setenv_nid_postfix(const char* name, const char* value, int overwrite) {
    return ::setenv(name, value, overwrite);
}

}
