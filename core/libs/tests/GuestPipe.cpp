#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#ifndef _WIN32
#include <csignal>
#endif
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "pipe check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr int GuestEfault = 14;

static void TestPipeValid() {
    int plain[2]{-1, -1};
    Require(pipe_nid_postfix(plain) == 0);
    Require(plain[0] >= 0 && plain[1] >= 0);
    Require(write_nid_postfix(plain[1], "test", 4) == 4);
    char buffer[8]{};
    Require(read_nid_postfix(plain[0], buffer, sizeof(buffer)) == 4);
    Require(std::memcmp(buffer, "test", 4) == 0);
    Require(close_nid_postfix(plain[0]) == 0 && close_nid_postfix(plain[1]) == 0);
}

static void TestPipeFaultAndReuse() {
    int first[2]{-1, -1};
    Require(pipe_nid_postfix(first) == 0);
    Require(close_nid_postfix(first[0]) == 0 && close_nid_postfix(first[1]) == 0);

    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
        *__error_nid_postfix() = 0;
        Require(pipe_nid_postfix(reinterpret_cast<int*>(address)) == -1 && *__error_nid_postfix() == GuestEfault);
    }

    *__error_nid_postfix() = 0;
    Require(pipe_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == GuestEfault);

    void* const readOnlyPage = mmap_nid_postfix(nullptr, 4096, 1, 0x1002, -1, 0);
    if (readOnlyPage != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1))) {
        *__error_nid_postfix() = 0;
        Require(pipe_nid_postfix(static_cast<int*>(readOnlyPage)) == -1 && *__error_nid_postfix() == GuestEfault);
        Require(munmap_nid_postfix(readOnlyPage, 4096) == 0);
    }

    int second[2]{-1, -1};
    Require(pipe_nid_postfix(second) == 0 && second[0] == first[0] && second[1] == first[1]);
    Require(close_nid_postfix(second[0]) == 0 && close_nid_postfix(second[1]) == 0);
}

int main() {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    TestPipeValid();
    TestPipeFaultAndReuse();
}
