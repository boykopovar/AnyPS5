#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

extern "C" int APS5_VABI sceHttpParseResponseHeader(const char*, std::size_t, const char*, const char**, std::size_t*);

static void Require(bool value) {
    if (!value) {
        std::fputs("HTTP response header check failed\n", stderr);
        std::abort();
    }
}

static void Check(std::string_view header, const char* field, std::size_t offset, std::string_view expected, int consumed) {
    const char* value = nullptr;
    std::size_t length = 999;
    const int result = sceHttpParseResponseHeader(header.data(), header.size(), field, &value, &length);
    if (result != consumed) {
        std::fprintf(stderr, "Field %s: consumed %d bytes, expected %d\n", field, result, consumed);
    }
    Require(result == consumed);
    Require(length == expected.size());
    Require(value == (expected.empty() ? nullptr : header.data() + offset));
    if (length != 0) Require(std::memcmp(value, expected.data(), length) == 0);
}

static void CheckGuardPage() {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
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
    constexpr std::string_view text = "X: abc\r\n\tdef\r\n";
    char* header = pages + pageSize - text.size();
    std::memcpy(header, text.data(), text.size());
    Check({header, text.size()}, "x", 3, "abc\r\n\tdef", static_cast<int>(text.size()));
    Check({header, 6}, "x", 3, "abc", 6);
    const char* value = header;
    std::size_t length = 123;
    Require(sceHttpParseResponseHeader(header, text.size(), "not-present", &value, &length) == static_cast<int>(0x80432025));
    Require(value == header && length == 123);
    Require(sceHttpParseResponseHeader(pages + pageSize, 0, "x", &value, &length) == static_cast<int>(0x80432025));
    pages[pageSize - 1] = 'X';
    Require(sceHttpParseResponseHeader(pages + pageSize - 1, 1, "x", &value, &length) == static_cast<int>(0x80432025));
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 2 * pageSize) == 0);
#endif
}

int main() {
    constexpr int invalidResponse = static_cast<int>(0x80432060);
    constexpr int invalidValue = static_cast<int>(0x804321FE);
    constexpr int notFound = static_cast<int>(0x80432025);
    Check("HTTP/1.1 200 OK\r\nX: \tvalue \t\r\nNext: no\r\n", "x", 21, "value \t", 30);
    Check("X: first\r\nX: second\r\n", "X", 3, "first", 10);
    Check("X: abc\n def\n\tghi\nY: no\n", "x", 3, "abc\n def\n\tghi", 17);
    Check("X: no-newline", "X", 3, "no-newline", 13);
    Check("X: value\r", "X", 3, "value\r", 9);
    Check("X:", "X", 2, "", 0);
    Check("X: \t", "X", 4, "", 0);
    Check("X:\r\n\r\n", "X", 5, "", 0);
    Check("X:\r\nnext\r\n", "X", 4, "next", 10);
    Check("Prefix: wrong\n X: wrong\nXy: wrong\nX: right\n", "X", 37, "right", 43);
    Check(std::string_view("X: a\0b\n", 7), "X", 3, std::string_view("a\0b", 3), 7);

    const char* marker = "unchanged";
    const char* value = marker;
    std::size_t length = 456;
    Require(sceHttpParseResponseHeader(nullptr, 1, "X", &value, &length) == invalidResponse);
    Require(sceHttpParseResponseHeader(nullptr, 0, nullptr, nullptr, nullptr) == invalidResponse);
    Require(sceHttpParseResponseHeader("X: yes", 6, nullptr, &value, &length) == invalidValue);
    Require(sceHttpParseResponseHeader("X: yes", 6, "X", nullptr, &length) == invalidValue);
    Require(sceHttpParseResponseHeader("X: yes", 6, "X", &value, nullptr) == invalidValue);
    Require(sceHttpParseResponseHeader("X: yes", 0, "X", &value, &length) == notFound);
    Require(sceHttpParseResponseHeader("X: yes", 1, "X", &value, &length) == notFound);
    Require(sceHttpParseResponseHeader("X: yes", 6, "Longer-Field", &value, &length) == notFound);
    Require(sceHttpParseResponseHeader("X: yes", 6, "X:", &value, &length) == notFound);
    Require(value == marker && length == 456);

    std::string field(0xfff, 'A');
    std::string header = field + ": value\n";
    field += "ignored";
    Check(header, field.c_str(), 0x1000 + 1, "value", static_cast<int>(header.size()));
    CheckGuardPage();
}
