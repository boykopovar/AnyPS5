#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int invalidResponse = static_cast<int>(0x80432060);
constexpr int invalidValue = static_cast<int>(0x804321FE);
constexpr int notFound = static_cast<int>(0x80432025);

struct HeaderExpectation {
    std::string_view header;
    const char* field;
    std::size_t offset;
    std::string_view expected;
    int consumed;
};

void RequireParsed(const HeaderExpectation& expectation) {
    const std::string context = "field " + std::string(expectation.field) + " in " + std::string(expectation.header.substr(0, 40));
    const char* value = nullptr;
    std::size_t length = 999;
    const int result = sceHttpParseResponseHeader(expectation.header.data(), expectation.header.size(), expectation.field, &value, &length);
    RequireEqual(result, expectation.consumed, context + ": consumed bytes");
    RequireEqual(length, expectation.expected.size(), context + ": value length");
    Require(value == (expectation.expected.empty() ? nullptr : expectation.header.data() + expectation.offset), context + ": value pointer");
    if (length != 0) Require(std::memcmp(value, expectation.expected.data(), length) == 0, context + ": value bytes");
}

class GuardedPages {
public:
    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        pages = static_cast<char*>(VirtualAlloc(nullptr, 2 * pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(pages != nullptr, "allocate two pages");
        DWORD oldProtection = 0;
        Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &oldProtection) != 0, "protect the guard page");
#else
        const long hostPageSize = sysconf(_SC_PAGESIZE);
        Require(hostPageSize > 0, "query the page size");
        pageSize = static_cast<std::size_t>(hostPageSize);
        void* mapping = mmap(nullptr, 2 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED, "map two pages");
        pages = static_cast<char*>(mapping);
        Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0, "protect the guard page");
#endif
    }

    ~GuardedPages() {
        if (pages == nullptr) return;
#ifdef _WIN32
        VirtualFree(pages, 0, MEM_RELEASE);
#else
        munmap(pages, 2 * pageSize);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    char* GuardStart() const {
        return pages + pageSize;
    }

    void Release() {
        char* const released = pages;
        pages = nullptr;
#ifdef _WIN32
        Require(VirtualFree(released, 0, MEM_RELEASE) != 0, "release the pages");
#else
        Require(munmap(released, 2 * pageSize) == 0, "unmap the pages");
#endif
    }

private:
    char* pages = nullptr;
    std::size_t pageSize = 0;
};

const Case fieldValues{"ParseResponseHeader_PresentField_ReturnsValueAndConsumedLength", [] {
    const HeaderExpectation expectations[] = {
        {"HTTP/1.1 200 OK\r\nX: \tvalue \t\r\nNext: no\r\n", "x", 21, "value \t", 30},
        {"X: first\r\nX: second\r\n", "X", 3, "first", 10},
        {"X: abc\n def\n\tghi\nY: no\n", "x", 3, "abc\n def\n\tghi", 17},
        {"X: no-newline", "X", 3, "no-newline", 13},
        {"X: value\r", "X", 3, "value\r", 9},
        {"X:\r\nnext\r\n", "X", 4, "next", 10},
        {"Prefix: wrong\n X: wrong\nXy: wrong\nX: right\n", "X", 37, "right", 43},
        {std::string_view("X: a\0b\n", 7), "X", 3, std::string_view("a\0b", 3), 7},
    };
    for (const auto& expectation : expectations) RequireParsed(expectation);
}};

const Case emptyValues{"ParseResponseHeader_EmptyFieldValue_ReturnsNullValueAndZeroConsumed", [] {
    const HeaderExpectation expectations[] = {
        {"X:", "X", 2, "", 0},
        {"X: \t", "X", 4, "", 0},
        {"X:\r\n\r\n", "X", 5, "", 0},
    };
    for (const auto& expectation : expectations) RequireParsed(expectation);
}};

const Case nullHeader{"ParseResponseHeader_NullHeader_ReturnsInvalidResponse", [] {
    const char* value = nullptr;
    std::size_t length = 0;
    RequireEqual(sceHttpParseResponseHeader(nullptr, 1, "X", &value, &length), invalidResponse, "null header");
    RequireEqual(sceHttpParseResponseHeader(nullptr, 0, nullptr, nullptr, nullptr), invalidResponse, "all null arguments");
}};

const Case nullOutputs{"ParseResponseHeader_NullFieldOrOutput_ReturnsInvalidValue", [] {
    const char* value = nullptr;
    std::size_t length = 0;
    RequireEqual(sceHttpParseResponseHeader("X: yes", 6, nullptr, &value, &length), invalidValue, "null field");
    RequireEqual(sceHttpParseResponseHeader("X: yes", 6, "X", nullptr, &length), invalidValue, "null value output");
    RequireEqual(sceHttpParseResponseHeader("X: yes", 6, "X", &value, nullptr), invalidValue, "null length output");
}};

const Case missingField{"ParseResponseHeader_FieldNotFound_ReturnsNotFoundWithoutWriting", [] {
    const char* marker = "unchanged";
    const char* value = marker;
    std::size_t length = 456;
    RequireEqual(sceHttpParseResponseHeader("X: yes", 0, "X", &value, &length), notFound, "empty header");
    RequireEqual(sceHttpParseResponseHeader("X: yes", 1, "X", &value, &length), notFound, "header cut after the name");
    RequireEqual(sceHttpParseResponseHeader("X: yes", 6, "Longer-Field", &value, &length), notFound, "longer field name");
    RequireEqual(sceHttpParseResponseHeader("X: yes", 6, "X:", &value, &length), notFound, "field name with a colon");
    Require(value == marker, "the value output was modified");
    RequireEqual(length, std::size_t{456}, "length output");
}};

const Case longField{"ParseResponseHeader_FieldLongerThan4095Characters_MatchesOnlyFirst4095", [] {
    std::string field(0xfff, 'A');
    const std::string header = field + ": value\n";
    field += "ignored";
    RequireParsed({header, field.c_str(), 0x1000 + 1, "value", static_cast<int>(header.size())});
}};

const Case guardPage{"ParseResponseHeader_HeaderEndingAtGuardPage_DoesNotReadPastEnd", [] {
    GuardedPages pages;
    constexpr std::string_view text = "X: abc\r\n\tdef\r\n";
    char* header = pages.GuardStart() - text.size();
    std::memcpy(header, text.data(), text.size());
    RequireParsed({{header, text.size()}, "x", 3, "abc\r\n\tdef", static_cast<int>(text.size())});
    RequireParsed({{header, 6}, "x", 3, "abc", 6});
    const char* value = header;
    std::size_t length = 123;
    RequireEqual(sceHttpParseResponseHeader(header, text.size(), "not-present", &value, &length), notFound, "missing field");
    Require(value == header, "the value output was modified");
    RequireEqual(length, std::size_t{123}, "length output");
    RequireEqual(sceHttpParseResponseHeader(pages.GuardStart(), 0, "x", &value, &length), notFound, "empty header at the guard page");
    pages.GuardStart()[-1] = 'X';
    RequireEqual(sceHttpParseResponseHeader(pages.GuardStart() - 1, 1, "x", &value, &length), notFound, "single byte before the guard page");
    pages.Release();
}};

} // namespace
