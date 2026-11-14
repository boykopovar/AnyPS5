#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>

namespace {
constexpr std::size_t Page = 0x4000;
constexpr int ReadWrite = 3;
constexpr int PrivateAnonymous = 0x1002;
constexpr int Invalid = 22;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "madvise check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

bool Succeeds(void* address, std::size_t length, int advice) {
    *__error_nid_postfix() = 77;
    return madvise_nid_postfix(address, length, advice) == 0 && *__error_nid_postfix() == 77;
}

bool PosixSucceeds(void* address, std::size_t length, int advice) {
    *__error_nid_postfix() = 77;
    return posix_madvise_nid_postfix(address, length, advice) == 0 && *__error_nid_postfix() == 77;
}

bool Fails(void* address, std::size_t length, int advice, int error) {
    *__error_nid_postfix() = 0;
    return madvise_nid_postfix(address, length, advice) == -1 && *__error_nid_postfix() == error;
}

bool PosixFails(void* address, std::size_t length, int advice, int error) {
    *__error_nid_postfix() = 0;
    return posix_madvise_nid_postfix(address, length, advice) == error;
}

char* Map(std::size_t pages) {
    void* mapped = mmap_nid_postfix(nullptr, pages * Page, ReadWrite, PrivateAnonymous, -1, 0);
    Require(mapped != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    return static_cast<char*>(mapped);
}
}

int main() {
    char* mapped = Map(2);
    mapped[0] = 1;

    for (int advice = 0; advice <= 10; ++advice) {
        Require(Succeeds(mapped, Page, advice));
        Require(PosixSucceeds(mapped, Page, advice));
    }

    Require(Succeeds(mapped + 100, 10, 0));
    Require(PosixSucceeds(mapped + 100, 10, 0));

    Require(Fails(mapped, Page, -1, Invalid));
    Require(Fails(mapped, Page, 11, Invalid));
    Require(PosixFails(mapped, Page, -1, Invalid));
    Require(PosixFails(mapped, Page, 11, Invalid));

    Require(Fails(reinterpret_cast<void*>(0), Page, 0, Invalid));
    Require(PosixFails(reinterpret_cast<void*>(0), Page, 0, Invalid));

    Require(munmap_nid_postfix(mapped, 2 * Page) == 0);
    return 0;
}
