#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libcurl/libcurl.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "curl string check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

template <typename TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct GuardedPages {
    char* base = nullptr;
    std::size_t page = 0;
};

static GuardedPages MapGuardedPages() {
    GuardedPages pages;
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    pages.page = info.dwPageSize;
    pages.base = static_cast<char*>(VirtualAlloc(nullptr, pages.page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD previous = 0;
    if (pages.base == nullptr || VirtualProtect(pages.base + pages.page, pages.page, PAGE_NOACCESS, &previous) == 0) {
        std::fputs("the guard page is not mapped\n", stderr);
        std::abort();
    }
#else
    const long pageSize = sysconf(_SC_PAGESIZE);
    pages.page = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 0;
    void* mapping = pages.page != 0 ? mmap(nullptr, pages.page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) : MAP_FAILED;
    pages.base = static_cast<char*>(mapping);
    if (mapping == MAP_FAILED || mprotect(pages.base + pages.page, pages.page, PROT_NONE) != 0) {
        std::fputs("the guard page is not mapped\n", stderr);
        std::abort();
    }
#endif
    return pages;
}

static void UnmapGuardedPages(const GuardedPages& pages) {
#ifdef _WIN32
    Check(VirtualFree(pages.base, 0, MEM_RELEASE) != 0);
#else
    Check(munmap(pages.base, pages.page * 2) == 0);
#endif
}

static void CheckEqual() {
    const char* same = "MixedCaseHeader";
    Check(curl_strequal(same, same) == 1);
    Check(curl_strequal("", "") == 1);
    Check(curl_strequal("Content-Type", "content-TYPE") == 1);
    Check(curl_strequal("azAZ", "AZaz") == 1);
    Check(curl_strequal("a1-_", "A1-_") == 1);
    Check(curl_strequal("abc", "abd") == 0);
    Check(curl_strequal("abc", "ab") == 0);
    Check(curl_strequal("ab", "abc") == 0);
    Check(curl_strequal("", "a") == 0);
    Check(curl_strequal("a", "") == 0);
    Check(curl_strequal("@", "`") == 0);
    Check(curl_strequal("[", "{") == 0);
    Check(curl_strequal("\xE9", "\xC9") == 0);
    Check(curl_strequal("\xE9", "\xE9") == 1);
}

static void CheckBounded() {
    Check(curl_strnequal("abc", "xyz", 0) == 1);
    Check(curl_strnequal("", "a", 0) == 1);
    Check(curl_strnequal("ab", "ax", 1) == 1);
    Check(curl_strnequal("abcX", "ABCy", 3) == 1);
    Check(curl_strnequal("abcX", "ABCy", 4) == 0);
    Check(curl_strnequal("abc", "ab", 2) == 1);
    Check(curl_strnequal("abc", "ab", 3) == 0);
    Check(curl_strnequal("ab", "abc", 3) == 0);
    Check(curl_strnequal("ab", "AB", 2) == 1);
    Check(curl_strnequal("ab", "AB", 100) == 1);
    Check(curl_strnequal("a\0x", "a\0y", 3) == 1);
    Check(curl_strnequal("ab\0x", "ab\0y", 4) == 1);
    Check(curl_strnequal("", "", 5) == 1);
    Check(curl_strnequal("\xE9", "\xC9", 1) == 0);
}

static void CheckUnmapped() {
    const auto* unmapped = reinterpret_cast<const char*>(0x10);
    Check(Throws([&] { curl_strequal(unmapped, "a"); }));
    Check(Throws([&] { curl_strequal("a", unmapped); }));
    Check(Throws([&] { curl_strnequal(unmapped, "a", 1); }));
    Check(Throws([&] { curl_strnequal("a", unmapped, 1); }));
    Check(curl_strnequal(unmapped, "a", 0) == 1);
    Check(curl_strnequal("a", unmapped, 0) == 1);
}

static void CheckLongStrings() {
    const std::string lower(1000, 'a');
    const std::string upper(1000, 'A');
    Check(curl_strequal(lower.c_str(), upper.c_str()) == 1);
    Check(curl_strnequal(lower.c_str(), upper.c_str(), 1000) == 1);
    std::string differs = upper;
    differs[600] = 'B';
    Check(curl_strequal(lower.c_str(), differs.c_str()) == 0);
    Check(curl_strnequal(lower.c_str(), differs.c_str(), 600) == 1);
    Check(curl_strnequal(lower.c_str(), differs.c_str(), 601) == 0);
    Check(curl_strequal(lower.c_str(), upper.substr(0, 999).c_str()) == 0);
}

static void CheckGuardPage() {
    const GuardedPages pages = MapGuardedPages();
    char* const terminated = pages.base + pages.page - 4;
    std::memcpy(terminated, "abc", 4);
    Check(curl_strequal(terminated, "ABC") == 1);
    Check(curl_strequal("ABC", terminated) == 1);
    Check(curl_strequal(terminated, "abd") == 0);
    Check(curl_strnequal(terminated, "ABC", 3) == 1);
    Check(curl_strnequal(terminated, "ABCD", 4) == 0);

    char* const unterminated = pages.base + pages.page - 4;
    std::memcpy(unterminated, "abcd", 4);
    Check(curl_strnequal(unterminated, "ABCD", 4) == 1);
    Check(curl_strnequal(unterminated, "ABCX", 3) == 1);
    Check(curl_strnequal(unterminated, "ABCX", 4) == 0);
    Check(Throws([&] { curl_strnequal(unterminated, "ABCD", 5); }));
    Check(Throws([&] { curl_strequal(unterminated, "abcd"); }));
    Check(Throws([&] { curl_strequal("abcd", unterminated); }));
    Check(curl_strequal(unterminated, "abcX") == 0);

    char* const longTail = pages.base + pages.page - 300;
    std::memset(longTail, 'a', 299);
    longTail[299] = '\0';
    const std::string expected(299, 'A');
    Check(curl_strequal(longTail, expected.c_str()) == 1);
    Check(curl_strequal(longTail, std::string(300, 'A').c_str()) == 0);
    longTail[299] = 'a';
    Check(curl_strequal(longTail, expected.c_str()) == 0);
    const std::string fullPage(300, 'A');
    Check(Throws([&] { curl_strequal(longTail, fullPage.c_str()); }));
    UnmapGuardedPages(pages);
}

int main() {
    CheckEqual();
    CheckBounded();
    CheckUnmapped();
    CheckLongStrings();
    CheckGuardPage();
    return 0;
}
