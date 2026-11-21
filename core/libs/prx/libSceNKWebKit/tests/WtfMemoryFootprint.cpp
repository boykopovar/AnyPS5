#include "prx/libSceNKWebKit/include/MemoryFootprintExports.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF memory footprint check failed at line %u\n", location.line());
        std::abort();
    }
}

}  // namespace

int main() {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    constexpr std::size_t allocationSize = 32 * 1024 * 1024;
    constexpr std::size_t minimumChange = allocationSize / 2;
    const auto before = _ZN3WTF15memoryFootprintEv();
    Require(before > 0);
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const std::size_t pageSize = system.dwPageSize;
    void* memory = VirtualAlloc(nullptr, allocationSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(memory != nullptr);
#else
    const long hostPageSize = sysconf(_SC_PAGESIZE);
    Require(hostPageSize > 0);
    const auto pageSize = static_cast<std::size_t>(hostPageSize);
    void* memory = mmap(nullptr, allocationSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(memory != MAP_FAILED);
#endif
    Require(pageSize > 0 && pageSize <= allocationSize);
    auto* bytes = static_cast<volatile unsigned char*>(memory);
    for (std::size_t offset = 0; offset < allocationSize; offset += pageSize) bytes[offset] = 1;
    const auto resident = _ZN3WTF15memoryFootprintEv();
    Require(resident >= before && resident - before >= minimumChange);
    Require(resident % pageSize == 0);
#ifdef _WIN32
    Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(memory, allocationSize) == 0);
#endif
    const auto released = _ZN3WTF15memoryFootprintEv();
    Require(released > 0 && released <= resident && resident - released >= minimumChange);
    std::printf("RSS bytes: before=%zu touched=%zu released=%zu\n", before, resident, released);
#else
    bool threw = false;
    try {
        static_cast<void>(_ZN3WTF15memoryFootprintEv());
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
#endif
}
