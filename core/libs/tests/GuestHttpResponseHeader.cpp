#include "prx/libSceHttp/include/Http.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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
    Check({header, text.size()}, "x", 3, "abc\r\n\tdef\r\n", static_cast<int>(text.size()));
    Check({header, 6}, "x", 3, "abc", 6);
    const char* value = header;
    std::size_t length = 123;
    Require(sceHttpParseResponseHeader(header, text.size(), "not-present", &value, &length) == static_cast<int>(0x80432025));
    Require(value == header && length == 123);
    Require(sceHttpParseResponseHeader(pages + pageSize, 0, "x", &value, &length) == static_cast<int>(0x80432025));
    errno = 71;
    Require(sceHttpParseResponseHeader(header, text.size() + 1, "x", &value, &length) == static_cast<int>(0x80432060));
    Require(errno == 71);
    const auto* const unreadableField = pages + pageSize - 1;
    Require(sceHttpParseResponseHeader("X: yes", 6, unreadableField, &value, &length) == static_cast<int>(0x804321FE));
    Require(errno == 71);
    Require(sceHttpParseResponseHeader(header, text.size(), "x", reinterpret_cast<const char**>(pages + pageSize), &length) == static_cast<int>(0x804321FE));
    Require(errno == 71);
    Require(sceHttpParseResponseHeader(header, text.size(), "x", &value, reinterpret_cast<std::size_t*>(pages + pageSize)) == static_cast<int>(0x804321FE));
    Require(errno == 71);
    Require(sceHttpParseResponseHeader("X:\r\nY: z", 9, "x", reinterpret_cast<const char**>(pages + pageSize), &length) == static_cast<int>(0x804321FE));
    Require(errno == 71);
    Require(sceHttpParseResponseHeader("X:\r\nY: z", 9, "x", &value, reinterpret_cast<std::size_t*>(pages + pageSize)) == static_cast<int>(0x804321FE));
    Require(errno == 71);
    pages[pageSize - 1] = 'X';
    Require(sceHttpParseResponseHeader(pages + pageSize - 1, 1, "x", &value, &length) == static_cast<int>(0x80432025));
    pages[pageSize - 2] = 'X';
    pages[pageSize - 1] = '\0';
    Check("X: yes", pages + pageSize - 2, 3, "yes", 6);
    const std::string longField(0xfff, 'A');
    std::memcpy(pages + pageSize - longField.size(), longField.data(), longField.size());
    const std::string longHeader = longField + ": yes";
    Check(longHeader, pages + pageSize - longField.size(), 0x1001, "yes", static_cast<int>(longHeader.size()));
#ifdef _WIN32
    Require(VirtualProtect(pages, pageSize, PAGE_READONLY, &oldProtection) != 0);
#else
    Require(mprotect(pages, pageSize, PROT_READ) == 0);
#endif
    errno = 71;
    Require(sceHttpParseResponseHeader("X: yes", 6, "x", reinterpret_cast<const char**>(pages), &length) == static_cast<int>(0x804321FE));
    Require(errno == 71);
    Require(sceHttpParseResponseHeader("X: yes", 6, "x", &value, reinterpret_cast<std::size_t*>(pages)) == static_cast<int>(0x804321FE));
    Require(errno == 71);
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
    Check("HTTP/1.1 200 OK\r\nX: \tvalue \t\r\nNext: no\r\n", "x", 21, "value \t", 28);
    Check("X: first\r\nX: second\r\n", "X", 3, "first", 8);
    Check("X: abc\n def\n\tghi\nY: no\n", "x", 3, "abc\n def\n\tghi", 16);
    Check("X: no-newline", "X", 3, "no-newline", 13);
    Check("X: value\r", "X", 3, "value\r", 9);
    Check("X:", "X", 2, "", 0);
    Check("X: \t", "X", 4, "", 0);
    Check("X:\r\n\r\n", "X", 5, "", 0);
    Check("X:\r\nnext\r\n", "X", 0, "", 0);
    Check("X:\n b\r\n", "X", 4, "b\r\n", 7);
    Check("X:\r\n\tb\r\nY: c", "X", 5, "b", 6);
    Check("Prefix: wrong\n X: wrong\nXy: wrong\nX: right\n", "X", 37, "right\n", 43);
    Check("HTTP/1.1 200 OK\r\nServer: x\r\n\r\nBody: no\r\n", "Body", 36, "no\r\n", 40);
    Check(" A: b\r\nB:  c  \r\n", "B", 11, "c  \r\n", 16);
    Check("Empty:\r\nSpaces:    \r\nNoSpace:v\r\nZ: z", "Empty", 0, "", 0);
    Check("Empty:\r\nSpaces:    \r\nNoSpace:v\r\nZ: z", "Spaces", 0, "", 0);
    Check("Empty:\r\nSpaces:    \r\nNoSpace:v\r\nZ: z", "NoSpace", 29, "v", 30);
    Check(std::string_view("X: a\0b\n", 7), "X", 3, std::string_view("a\0b\n", 4), 7);
    Check(std::string_view("X: a\0b\r\nY: z", 13), "X", 3, std::string_view("a\0b", 3), 6);

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

    errno = 71;
    Require(sceHttpParseResponseHeader("X: yes", std::numeric_limits<std::size_t>::max(), "X", &value, &length) == invalidResponse);
    Require(value == marker && length == 456 && errno == 71);
    Require(sceHttpParseResponseHeader("X: yes", static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) + 1, "X", &value, &length) == invalidResponse);
    Require(value == marker && length == 456 && errno == 71);

    const std::string multiChunk = "Other: " + std::string(4088, 'a') + "\nX: abc\r\n " + std::string(8192, 'b') + "\r\n";
    Check(multiChunk, "X", 4099, std::string_view(multiChunk).substr(4099), static_cast<int>(multiChunk.size()));

    const auto* const unreadable = reinterpret_cast<const char*>(0x10);
    errno = 71;
    Require(sceHttpParseResponseHeader(unreadable, 6, "X", &value, &length) == invalidResponse);
    Require(value == marker && length == 456 && errno == 71);
    Require(sceHttpParseResponseHeader("X: yes", 6, unreadable, &value, &length) == invalidValue);
    Require(value == marker && length == 456 && errno == 71);
    Require(sceHttpParseResponseHeader("Y: yes", 6, "X", reinterpret_cast<const char**>(0x10), reinterpret_cast<std::size_t*>(0x10)) == notFound);
    Require(errno == 71);

    std::string field(0xfff, 'A');
    std::string header = field + ": value\n";
    field += "ignored";
    Check(header, field.c_str(), 0x1000 + 1, "value\n", static_cast<int>(header.size()));
    CheckGuardPage();
}
