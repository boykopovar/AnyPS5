#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ctermid check failed at line %u\n", location.line());
        std::abort();
    }
}

}

int main() {
    char* shared = ctermid_nid_postfix(nullptr);
    Require(shared != nullptr);
    Require(std::strcmp(shared, "/dev/tty") == 0);

    char buffer[1024];
    std::memset(buffer, 'x', sizeof(buffer));
    char* returned = ctermid_nid_postfix(buffer);
    Require(returned == buffer);
    Require(std::strcmp(buffer, "/dev/tty") == 0);
    Require(buffer[8] == '\0' && buffer[9] == 'x');

#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READONLY);
    Require(page != nullptr);
#else
    const auto size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* page = mmap(nullptr, size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    *__error_nid_postfix() = 0;
    Require(ctermid_nid_postfix(static_cast<char*>(page)) == nullptr);
    Require(*__error_nid_postfix() == 14);
    return 0;
}
