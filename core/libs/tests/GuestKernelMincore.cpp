#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <vector>

namespace {
constexpr std::size_t Page = 0x4000;
constexpr int ReadWrite = 3;
constexpr int PrivateAnonymous = 0x1002;
constexpr int Invalid = 22;
constexpr int NoMemory = 12;
constexpr int Fault = 14;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "mincore check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

bool Succeeds(const void* address, std::size_t length, char* vec) {
    *__error_nid_postfix() = 77;
    return mincore_nid_postfix(address, length, vec) == 0 && *__error_nid_postfix() == 77;
}

bool Fails(const void* address, std::size_t length, char* vec, int error) {
    *__error_nid_postfix() = 0;
    return mincore_nid_postfix(address, length, vec) == -1 && *__error_nid_postfix() == error;
}

char* Map(std::size_t pages) {
    void* mapped = mmap_nid_postfix(nullptr, pages * Page, ReadWrite, PrivateAnonymous, -1, 0);
    Require(mapped != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    return static_cast<char*>(mapped);
}
}

int main() {
    char* mapped = Map(3);
    mapped[0] = 1;
    mapped[Page] = 2;

    std::vector<char> vec(3, 0);
    Require(Succeeds(mapped, 3 * Page, vec.data()));
    Require(vec[0] == 1);
    Require(vec[1] == 1);
    Require(vec[2] == 0);

    Require(Succeeds(mapped + 100, 10, vec.data()));
    Require(vec[0] == 1);

    Require(Succeeds(mapped, 0, vec.data()));

    const auto top = std::numeric_limits<std::uintptr_t>::max() & ~(Page - 1);
    Require(Fails(reinterpret_cast<void*>(top), 2 * Page, vec.data(), NoMemory));
    Require(Fails(mapped, std::numeric_limits<std::size_t>::max() - (Page - 1) + 1, vec.data(), NoMemory));
    Require(Fails(mapped + 1, std::numeric_limits<std::size_t>::max() - (Page - 1), vec.data(), NoMemory));

    Require(munmap_nid_postfix(mapped, Page) == 0);
    Require(Fails(mapped, 2 * Page, vec.data(), NoMemory));

    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, 2 * Page, 0, 0) == 0);
    std::vector<char> reservedVec(2, 0);
    Require(Succeeds(reserved, 2 * Page, reservedVec.data()));
    Require(reservedVec[0] == 0);
    Require(reservedVec[1] == 0);

    Require(munmap_nid_postfix(mapped + Page, 2 * Page) == 0);
    Require(munmap_nid_postfix(reserved, 2 * Page) == 0);

#ifdef _WIN32
    void* badPage = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
    Require(badPage != nullptr);
    std::vector<char> badVec(1, 0);
    Require(Fails(badPage, Page, badVec.data(), Fault));
    VirtualFree(badPage, 0, MEM_RELEASE);
#else
    void* badPage = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(badPage != MAP_FAILED);
    std::vector<char> badVec(1, 0);
    Require(Fails(badPage, Page, badVec.data(), Fault));
    munmap(badPage, 4096);
#endif

    return 0;
}
