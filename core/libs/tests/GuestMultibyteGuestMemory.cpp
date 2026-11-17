#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::size_t Oversized = std::size_t{1} << 40;
constexpr auto Incomplete = static_cast<std::size_t>(-2);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "multibyte guest memory: %s\n", message);
    std::abort();
}

template<typename TCall>
void RequireInvalid(TCall call, const char* text) {
    bool threw = false;
    errno = 71;
    try {
        call();
    } catch (const std::invalid_argument& error) {
        threw = std::strstr(error.what(), text) != nullptr;
    }
    Require(threw && errno == 71, "invalid guest memory reports the expected failure without changing errno");
}

class GuardedPages {
public:
    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        memory = static_cast<std::byte*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr, "guarded allocation succeeds");
        DWORD previous = 0;
        Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "guard page protection succeeds");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "host page size is available");
        pageSize = static_cast<std::size_t>(size);
        void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED, "guarded allocation succeeds");
        memory = static_cast<std::byte*>(allocation);
        Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0, "guard page protection succeeds");
#endif
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, pageSize * 2);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    template<typename TElement>
    TElement* Last(std::size_t count) { return reinterpret_cast<TElement*>(memory + pageSize) - count; }

    void ReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(memory, pageSize, PAGE_READONLY, &previous) != 0, "read-only protection succeeds");
#else
        Require(mprotect(memory, pageSize, PROT_READ) == 0, "read-only protection succeeds");
#endif
    }

private:
    std::byte* memory = nullptr;
    std::size_t pageSize = 0;
};

void* Inaccessible() {
    return reinterpret_cast<void*>(std::uintptr_t{0x10});
}

void CheckMbrtoc() {
    GuardedPages pages;
    char* bytes = pages.Last<char>(2);
    std::memcpy(bytes, "\xc3\xa9", 2);
    std::uint64_t state[2] = {};
    std::uint32_t wide = 0;
    std::uint16_t unit = 0;
    Require(mbrtoc32_nid_postfix(&wide, bytes, Oversized, state) == 2 && wide == 0xe9, "mbrtoc32 stops reading at the end of a character before a guard page");
    Require(mbrtoc16_nid_postfix(&unit, bytes, Oversized, state) == 2 && unit == 0xe9, "mbrtoc16 stops reading at the end of a character before a guard page");
    Require(mbrtoc32_nid_postfix(&wide, bytes, 1, state) == Incomplete && reinterpret_cast<unsigned char*>(state)[10] == 1, "mbrtoc32 stores an incomplete character in the state");
    Require(mbrtoc32_nid_postfix(&wide, bytes + 1, 1, state) == 1 && wide == 0xe9, "mbrtoc32 completes the character from the state");
    bytes[1] = '\xc3';
    RequireInvalid([&] { mbrtoc32_nid_postfix(&wide, bytes + 1, Oversized, state); }, "mbrtoc32: unreadable source");
    state[0] = state[1] = 0;
    RequireInvalid([&] { mbrtoc16_nid_postfix(&unit, bytes + 1, Oversized, state); }, "mbrtoc16: unreadable source");
    state[0] = state[1] = 0;

    char* text = pages.Last<char>(3);
    std::memcpy(text, "\xe2\x82\xac", 3);
    Require(mbrtoc16_nid_postfix(&unit, text, Oversized, state) == 3 && unit == 0x20ac, "mbrtoc16 decodes a character that ends at a guard page");

    RequireInvalid([&] { mbrtoc32_nid_postfix(&wide, "A", 1, Inaccessible()); }, "mbrtoc32: unreadable conversion state");
    RequireInvalid([&] { mbrtoc16_nid_postfix(&unit, "A", 1, Inaccessible()); }, "mbrtoc16: unreadable conversion state");
    RequireInvalid([&] { mbrtoc32_nid_postfix(static_cast<std::uint32_t*>(Inaccessible()), "A", 1, state); }, "mbrtoc32: unwritable destination");
    RequireInvalid([&] { mbrtoc16_nid_postfix(static_cast<std::uint16_t*>(Inaccessible()), "A", 1, state); }, "mbrtoc16: unwritable destination");
    Require(mbrtoc32_nid_postfix(nullptr, "A", 1, state) == 1, "mbrtoc32 skips a null destination");

    GuardedPages statePages;
    auto* readOnly = statePages.Last<unsigned char>(16);
    statePages.ReadOnly();
    RequireInvalid([&] { mbrtoc32_nid_postfix(&wide, "A", 1, readOnly); }, "mbrtoc32: unwritable conversion state");
    RequireInvalid([&] { mbrtoc16_nid_postfix(&unit, "\xc3", 1, readOnly); }, "mbrtoc16: unwritable conversion state");
}

void CheckWcsrtombs() {
    GuardedPages pages;
    std::uint16_t* wide = pages.Last<std::uint16_t>(3);
    wide[0] = 'a';
    wide[1] = 0xe9;
    wide[2] = 0;
    char output[8];
    const std::uint16_t* source = wide;
    std::memset(output, 'x', sizeof(output));
    Require(wcsrtombs_nid_postfix(output, &source, Oversized, nullptr) == 3 && source == nullptr && std::memcmp(output, "a\xc3\xa9\0x", 5) == 0, "wcsrtombs reads a string that ends at a guard page");
    source = wide;
    Require(wcsrtombs_nid_postfix(nullptr, &source, 0, nullptr) == 3 && source == wide, "wcsrtombs measures a string that ends at a guard page");
    source = wide + 1;
    Require(wcsrtombs_nid_postfix(output, &source, 1, nullptr) == 0 && source == wide + 1, "wcsrtombs leaves the source at a character that does not fit");
    wide[2] = 'b';
    source = wide;
    RequireInvalid([&] { wcsrtombs_nid_postfix(output, &source, Oversized, nullptr); }, "wcsrtombs: unreadable source");
    RequireInvalid([&] { wcsrtombs_nid_postfix(nullptr, static_cast<const std::uint16_t**>(Inaccessible()), 0, nullptr); }, "wcsrtombs: unreadable source pointer");
    wide[2] = 0;

    GuardedPages outputPages;
    char* destination = outputPages.Last<char>(4);
    source = wide;
    Require(wcsrtombs_nid_postfix(destination, &source, Oversized, nullptr) == 3 && source == nullptr && std::memcmp(destination, "a\xc3\xa9", 4) == 0, "wcsrtombs writes up to a guard page");
    source = wide;
    Require(wcsrtombs_nid_postfix(destination + 1, &source, 3, nullptr) == 3 && source == wide + 2, "wcsrtombs stops at the capacity before a guard page");
    source = wide;
    RequireInvalid([&] { wcsrtombs_nid_postfix(destination + 1, &source, Oversized, nullptr); }, "wcsrtombs: unwritable destination");

    GuardedPages pointerPages;
    auto** stored = pointerPages.Last<const std::uint16_t*>(1);
    *stored = wide;
    pointerPages.ReadOnly();
    Require(wcsrtombs_nid_postfix(nullptr, stored, 0, nullptr) == 3, "wcsrtombs measures through a read-only source pointer");
    RequireInvalid([&] { wcsrtombs_nid_postfix(output, stored, sizeof(output), nullptr); }, "wcsrtombs: unwritable source pointer");
}

void CheckWcstombs() {
    GuardedPages pages;
    std::uint16_t* wide = pages.Last<std::uint16_t>(3);
    wide[0] = 'a';
    wide[1] = 0x20ac;
    wide[2] = 0;
    char output[8];
    std::memset(output, 'x', sizeof(output));
    Require(wcstombs_nid_postfix(output, wide, Oversized) == 4 && std::memcmp(output, "a\xe2\x82\xac\0x", 6) == 0, "wcstombs reads a string that ends at a guard page");
    Require(wcstombs_nid_postfix(nullptr, wide, 0) == 4, "wcstombs measures a string that ends at a guard page");
    wide[2] = 'b';
    RequireInvalid([&] { wcstombs_nid_postfix(output, wide, Oversized); }, "wcstombs: unreadable source");
    RequireInvalid([&] { wcstombs_nid_postfix(nullptr, static_cast<const std::uint16_t*>(Inaccessible()), 0); }, "wcstombs: unreadable source");
    wide[2] = 0;

    GuardedPages outputPages;
    char* destination = outputPages.Last<char>(5);
    Require(wcstombs_nid_postfix(destination, wide, Oversized) == 4 && std::memcmp(destination, "a\xe2\x82\xac", 5) == 0, "wcstombs writes up to a guard page");
    Require(wcstombs_nid_postfix(destination + 2, wide, 3) == 3 && std::memcmp(destination + 2, "a\xe2\x82", 3) == 0, "wcstombs stops at the capacity before a guard page");
    RequireInvalid([&] { wcstombs_nid_postfix(destination + 2, wide, Oversized); }, "wcstombs: unwritable destination");
    Require(wcstombs_nid_postfix(static_cast<char*>(Inaccessible()), wide, 0) == 0, "wcstombs writes nothing at zero capacity");
}

void CheckLongStrings() {
    std::vector<std::uint16_t> wide;
    std::string expected;
    for (std::size_t index = 0; index < 5000; ++index) {
        const std::uint16_t unit = index % 3 == 0 ? 0x20ac : index % 3 == 1 ? 0xe9 : static_cast<std::uint16_t>('a' + index % 26);
        wide.push_back(unit);
        expected += unit == 0x20ac ? "\xe2\x82\xac" : unit == 0xe9 ? "\xc3\xa9" : std::string(1, static_cast<char>(unit));
    }
    wide.push_back(0);
    std::vector<char> output(expected.size() + 2, 'x');
    const std::uint16_t* source = wide.data();
    Require(wcsrtombs_nid_postfix(output.data(), &source, Oversized, nullptr) == expected.size() && source == nullptr, "wcsrtombs converts a string longer than its buffers");
    Require(std::memcmp(output.data(), expected.c_str(), expected.size() + 1) == 0 && output.back() == 'x', "wcsrtombs writes every byte of a long string");
    std::fill(output.begin(), output.end(), 'x');
    Require(wcstombs_nid_postfix(output.data(), wide.data(), expected.size() - 1) == expected.size() - 1, "wcstombs fills a long capacity");
    Require(std::memcmp(output.data(), expected.data(), expected.size() - 1) == 0 && output[expected.size() - 1] == 'x', "wcstombs writes exactly its capacity of a long string");
}

void WarmUpHostExceptionRuntime() {
    try {
        throw std::runtime_error("warm-up");
    } catch (const std::runtime_error&) {
    }
}

}

int main() {
    WarmUpHostExceptionRuntime();
    CheckMbrtoc();
    CheckWcsrtombs();
    CheckWcstombs();
    CheckLongStrings();
    std::puts("multibyte guest memory checks passed");
    return 0;
}
