#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "asctime pointer guard failed: %s\n", message);
        std::abort();
    }
}

class Pages {
    unsigned char* allocation;
    std::size_t size;
public:
    Pages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(allocation != nullptr, "allocate pages");
        DWORD previous;
        Require(VirtualProtect(allocation + size, size, PAGE_NOACCESS, &previous) != 0, "protect page");
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0, "page size");
        size = static_cast<std::size_t>(page);
        allocation = static_cast<unsigned char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(allocation != MAP_FAILED, "allocate pages");
        Require(mprotect(allocation + size, size, PROT_NONE) == 0, "protect page");
#endif
        std::memset(allocation, 0xa5, size);
        char value = 0;
        Require(!GuestCopyIn_nid_no_patch(&value, allocation + size, 1), "qualify unreadable page");
        Require(!GuestCopyOut_nid_no_patch(allocation + size, &value, 1), "qualify unwritable page");
    }
    ~Pages() {
#ifdef _WIN32
        Require(VirtualFree(allocation, 0, MEM_RELEASE) != 0, "release pages");
#else
        Require(munmap(allocation, size * 2) == 0, "release pages");
#endif
    }
    unsigned char* End() { return allocation + size; }
    void ReadOnly() {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(allocation, size, PAGE_READONLY, &previous) != 0, "read-only page");
#else
        Require(mprotect(allocation, size, PROT_READ) == 0, "read-only page");
#endif
    }
};

GuestTm Time() {
    GuestTm result{};
    result.tm_year = 126;
    result.tm_mon = 9;
    result.tm_mday = 2;
    result.tm_hour = 13;
    result.tm_min = 5;
    result.tm_sec = 9;
    result.tm_wday = 5;
    return result;
}

void Valid() {
    auto time = Time();
    std::array<char, 64> output;
    output.fill('#');
    *__error_nid_postfix() = 73;
    Require(asctime_r_nid_postfix(&time, output.data()) == output.data(), "valid return pointer");
    Require(std::strcmp(output.data(), "Fri Oct  2 13:05:09 2026\n") == 0, "valid formatting");
    Require(*__error_nid_postfix() == 73 && output[26] == '#', "valid errno and canary");
    Require(std::strcmp(asctime_nid_postfix(&time), output.data()) == 0, "thread-local alias");
    output.fill('#');
    time.tm_wday = 100;
    Require(asctime_r_nid_postfix(&time, output.data()) == output.data(), "empty name return");
    const auto used = std::strlen(output.data()) + 1;
    for (std::size_t index = used; index < output.size(); ++index)
        Require(output[index] == '#', "short formatted output preserves tail");
    *__error_nid_postfix() = 73;
    Require(asctime_r_nid_postfix(nullptr, output.data()) == output.data(), "null time return");
    Require(std::strcmp(output.data(), "??? ??? ?? ??:??:?? ????\n") == 0 && *__error_nid_postfix() == 22, "null time policy");
    Pages pages;
    bool threw = false;
    try { asctime_r_nid_postfix(reinterpret_cast<const GuestTm*>(pages.End()), nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw, "null output precedes unreadable time");
    time = Time();
    std::memcpy(pages.End() - 32, &time, 28);
    Require(asctime_r_nid_postfix(reinterpret_cast<const GuestTm*>(pages.End() - 32), output.data()) == output.data(), "only formatter fields are read");
    Require(std::strcmp(output.data(), "Fri Oct  2 13:05:09 2026\n") == 0, "boundary input formatting");
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "test mode");
    const std::string_view mode = argv[1];
    if (mode == "valid") {
        Valid();
        return 0;
    }
    Pages pages;
    auto time = Time();
    std::array<char, 32> output;
    output.fill('#');
    if (mode == "read") {
        std::memcpy(pages.End() - 24, &time, 24);
        std::fputs("asctime read guard qualified=1\n", stderr);
        std::fflush(stderr);
        *__error_nid_postfix() = 73;
        Require(asctime_r_nid_postfix(reinterpret_cast<const GuestTm*>(pages.End() - 24), output.data()) == nullptr, "unreadable input return");
        Require(*__error_nid_postfix() == 14, "unreadable input errno");
        for (char value : output) Require(value == '#', "unreadable input leaves output untouched");
    } else if (mode == "write" || mode == "readonly") {
        char* destination = reinterpret_cast<char*>(pages.End() - (mode == "write" ? 25 : 32));
        if (mode == "readonly") pages.ReadOnly();
        const std::array<char, 26> probe{};
        Require(!GuestCopyOut_nid_no_patch(destination, probe.data(), probe.size()), "qualify output span");
        std::fputs("asctime write guard qualified=1\n", stderr);
        std::fflush(stderr);
        *__error_nid_postfix() = 73;
        Require(asctime_r_nid_postfix(&time, destination) == nullptr, "unwritable output return");
        Require(*__error_nid_postfix() == 14, "unwritable output errno");
    } else {
        Require(false, "unknown mode");
    }
}
