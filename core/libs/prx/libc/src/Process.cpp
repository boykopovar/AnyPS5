#include <cstdint>
#include <cstdlib>
#include <cstring>
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
    if (func == nullptr)
        return 0;
    return std::atexit(func);
}

int setenv_nid_postfix(const char* name, const char* value, int overwrite) {
    if (overwrite == 0 && std::getenv(name) != nullptr) {
        return 0;
    }
    const size_t nameLength = std::strlen(name);
    const size_t valueLength = std::strlen(value);
    char* entry = static_cast<char*>(std::malloc(nameLength + valueLength + 2));
    if (entry == nullptr) {
        errno = ENOMEM;
        return -1;
    }
    std::memcpy(entry, name, nameLength);
    entry[nameLength] = '=';
    std::memcpy(entry + nameLength + 1, value, valueLength);
    entry[nameLength + 1 + valueLength] = '\0';
    return ::putenv(entry);
}

}
