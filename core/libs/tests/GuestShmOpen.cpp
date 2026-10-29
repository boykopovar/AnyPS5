#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }
static const char* const anonymous = reinterpret_cast<const char*>(1);
static void RequireInvalid(const char* path, int flags) {
    *__error_nid_postfix() = 0;
    Require(shm_open_nid_postfix(path, flags, 0600) == -1);
    Require(*__error_nid_postfix() == 22);
}
int main() {
    *__error_nid_postfix() = 13;
    const int first = shm_open_nid_postfix(anonymous, 0x100202, 0600);
    const int second = shm_open_nid_postfix(anonymous, 0x2, 0);
    const int third = shm_open_nid_postfix(anonymous, 0x100e02, 0400);
    Require(first >= 0 && second >= 0 && third >= 0);
    Require(first != second && second != third && first != third);
    Require(*__error_nid_postfix() == 13);
    Require(ftruncate_nid_postfix(first, 0x4000) == 0);
    const char text[] = "anonymous";
    Require(pwrite_nid_disambig1_nid_postfix(first, text, sizeof(text), 0x1000) == static_cast<std::int64_t>(sizeof(text)));
    char back[sizeof(text)] = {};
    Require(pread_nid_postfix(first, back, sizeof(back), 0x1000) == static_cast<std::int64_t>(sizeof(back)));
    Require(std::memcmp(back, text, sizeof(text)) == 0);
    char last = 1;
    Require(pread_nid_postfix(first, &last, 1, 0x3fff) == 1 && last == 0);
    Require(pread_nid_postfix(first, &last, 1, 0x4000) == 0);
    Require(pread_nid_postfix(second, back, sizeof(back), 0) == 0);
    auto* view = static_cast<volatile char*>(mmap_nid_postfix(nullptr, 0x4000, 3, 0x1, first, 0));
    Require(view != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    Require(view[0x1000] == 'a' && view[0x3fff] == 0);
    view[0x2000] = 'm';
    Require(pread_nid_postfix(first, &last, 1, 0x2000) == 1 && last == 'm');
    Require(munmap_nid_postfix(const_cast<char*>(view), 0x4000) == 0);
    const unsigned char code[] = {0xb8, 0x2a, 0, 0, 0, 0xc3};
    Require(pwrite_nid_disambig1_nid_postfix(first, code, sizeof(code), 0) == static_cast<std::int64_t>(sizeof(code)));
    void* executable = mmap_nid_postfix(nullptr, 0x4000, 5, 0x1, first, 0);
    Require(executable != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    int (*function)();
    std::memcpy(&function, &executable, sizeof(function));
    Require(function() == 42);
    Require(munmap_nid_postfix(executable, 0x4000) == 0);
#ifndef _WIN32
    Require(pwrite_nid_disambig1_nid_postfix(third, text, sizeof(text), 0) == 0);
#endif
    Require(ftruncate_nid_postfix(third, 0x4000) == 0);
    Require(pwrite_nid_disambig1_nid_postfix(third, text, sizeof(text), 0) == static_cast<std::int64_t>(sizeof(text)));
    Require(close_nid_postfix(first) == 0 && close_nid_postfix(second) == 0 && close_nid_postfix(third) == 0);
    RequireInvalid(anonymous, 0x0);
    RequireInvalid(anonymous, 0x200);
    RequireInvalid(anonymous, 0x1);
    RequireInvalid(anonymous, 0x3);
    RequireInvalid(anonymous, 0x2 | 0x8);
    RequireInvalid(anonymous, 0x2 | 0x4);
    RequireInvalid("/anyps5-shm", 0x1);
    RequireInvalid("/anyps5-shm", 0x202 | 0x8);
#ifdef _WIN32
    RequireInvalid("", 0x202);
#else
    RequireInvalid("", 0x202);
#endif
}
