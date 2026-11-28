#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "CopyGuestString check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static char* ReadablePageBeforeGuard() {
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const std::size_t pageSize = system.dwPageSize;
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, 2 * pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr);
    DWORD oldProtection = 0;
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &oldProtection) != 0);
#else
    const long hostPageSize = sysconf(_SC_PAGESIZE);
    Require(hostPageSize > 0);
    const std::size_t pageSize = static_cast<std::size_t>(hostPageSize);
    auto* pages = static_cast<char*>(mmap(nullptr, 2 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(pages != MAP_FAILED);
    Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0);
#endif
    std::memset(pages, 'x', pageSize);
    return pages + pageSize;
}

int main() {
    std::string copy = "unchanged";
    Require(CopyGuestString_nid_no_patch(nullptr, 16, copy) == 14 && copy == "unchanged");
    Require(CopyGuestString_nid_no_patch(reinterpret_cast<const char*>(0x10), 16, copy) == 14 && copy == "unchanged");
    Require(CopyGuestString_nid_no_patch("guest", 0, copy) == 63 && copy == "unchanged");
    Require(CopyGuestString_nid_no_patch("guest", 5, copy) == 63 && copy == "unchanged");
    Require(CopyGuestString_nid_no_patch("guest", 6, copy) == 0 && copy == "guest");
    Require(CopyGuestString_nid_no_patch("", 1, copy) == 0 && copy.empty());
    Require(CopyGuestString_nid_no_patch("guest", 16, copy) == 0 && copy == "guest");

    const std::string longText(100000, 'x');
    Require(CopyGuestString_nid_no_patch(longText.c_str(), longText.size(), copy) == 63);
    Require(CopyGuestString_nid_no_patch(longText.c_str(), longText.size() + 1, copy) == 0 && copy == longText);

    const std::size_t unbounded = std::numeric_limits<std::size_t>::max() / 2;
    Require(CopyGuestString_nid_no_patch("guest", unbounded, copy) == 0 && copy == "guest");
    Require(CopyGuestString_nid_no_patch(longText.c_str(), unbounded, copy) == 0 && copy == longText);

    char* const guard = ReadablePageBeforeGuard();
    std::memcpy(guard - 4, "abc", 4);
    Require(CopyGuestString_nid_no_patch(guard - 4, unbounded, copy) == 0 && copy == "abc");
    Require(CopyGuestString_nid_no_patch(guard - 4, 4, copy) == 0 && copy == "abc");
    std::memcpy(guard - 4, "abcd", 4);
    copy = "unchanged";
    Require(CopyGuestString_nid_no_patch(guard - 4, 4, copy) == 63 && copy == "unchanged");
    Require(CopyGuestString_nid_no_patch(guard - 4, 5, copy) == 14 && copy == "unchanged");
    Require(CopyGuestString_nid_no_patch(guard - 4, unbounded, copy) == 14 && copy == "unchanged");
}
