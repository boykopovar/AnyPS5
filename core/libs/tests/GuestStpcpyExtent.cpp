#include "prx/libc/include/Api.hpp"
#include "tests/GuardedBuffer.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestStpcpyExtent check failed\n", stderr);
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

template<class T>
static void Prefix(const T* data) {
    for (int index = 1; index <= 8; ++index) Require(data[-index] == static_cast<T>(sizeof(T) == 1 ? 0xa5 : 0xa5a5));
}

static char* UnwritablePage() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, static_cast<std::size_t>(sysconf(_SC_PAGESIZE)), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return static_cast<char*>(page);
}

static void UnwritableDestination() {
    char* const page = UnwritablePage();
    const char* unusable[] = {reinterpret_cast<const char*>(0x10), page};
    const char source[] = "stpcpy unwritable destination payload";
    for (const char* destination : unusable) {
        *__error_nid_postfix() = 0;
        Require(stpcpy_nid_postfix(const_cast<char*>(destination), source) == nullptr);
        Require(*__error_nid_postfix() == 14);
    }
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

int main() {
    UnwritableDestination();
    GuardedBuffer input, output;
    for (std::size_t count = 0; count <= 192; ++count) {
        auto* source = input.At<char>(count + 1);
        auto* destination = output.At<char>(count + 1);
        for (std::size_t index = 0; index < count; ++index) source[index] = static_cast<char>(1 + (index * 97 + count) % 255);
        source[count] = 0;
        const std::string saved(source, count + 1);
        std::memset(destination - 16, 0xa5, count + 17);
        char* end = stpcpy_nid_postfix(destination, source);
        Require(end == destination + count && *end == 0);
        Require(std::equal(saved.begin(), saved.end(), destination));
        Require(std::equal(saved.begin(), saved.end(), source));
        Require(stpcpy_nid_postfix(end, "") == end);
        Prefix(reinterpret_cast<unsigned char*>(destination));
    }
}
