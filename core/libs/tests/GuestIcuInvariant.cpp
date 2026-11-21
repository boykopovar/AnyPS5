#include "prx/libSceIcu/libSceIcu.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU invariant: %s\n", message);
    std::abort();
}

template<class TCall>
void RequiresRuntimeErrorKeepingErrno(TCall call, const char* message) {
    errno = 71;
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == 71, message);
}

void Place(char* destination, const char16_t* units, std::size_t count) {
    std::memcpy(destination, units, count * sizeof(char16_t));
}

void GuestPointers() {
#ifdef _WIN32
    const std::size_t pageSize = 4096;
    auto* base = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(base != nullptr, "allocate guest pointer pages");
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
    void* mapping = mmap(nullptr, pageSize * 4, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "allocate guest pointer pages");
    auto* base = static_cast<char*>(mapping);
#endif
    char* pages = base + pageSize;
    char* guard = pages + pageSize;
    char* readOnly = pages + pageSize * 2;
    Place(pages - 1, u"xyz", 4);
    std::memset(readOnly, 'R', 16);
#ifdef _WIN32
    DWORD oldProtect = 0;
    Require(VirtualProtect(guard, pageSize, PAGE_NOACCESS, &oldProtect) != 0, "protect guard page");
    Require(VirtualProtect(readOnly, pageSize, PAGE_READONLY, &oldProtect) != 0, "protect read-only page");
#else
    Require(mprotect(guard, pageSize, PROT_NONE) == 0, "protect guard page");
    Require(mprotect(readOnly, pageSize, PROT_READ) == 0, "protect read-only page");
#endif
    const auto* straddling = reinterpret_cast<const char16_t*>(pages - 1);
    const auto* unmappedUnits = reinterpret_cast<const char16_t*>(std::uintptr_t{0x10});
    const auto* unmappedChars = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    auto* unmappedOut = reinterpret_cast<char*>(std::uintptr_t{0x10});
    auto* unmappedUnitsOut = reinterpret_cast<char16_t*>(std::uintptr_t{0x10});
    char chars[8];
    char16_t units[4];

    Place(guard - 8, u"Ab_", 4);
    const auto* terminated = reinterpret_cast<const char16_t*>(guard - 8);
    std::memset(chars, 'X', sizeof(chars));
    Require(u_austrcpy_nid_postfix(chars, terminated) == chars && std::memcmp(chars, "Ab_\0XXXX", 8) == 0, "terminated copy stops before the guard page");
    std::memset(chars, 'X', sizeof(chars));
    u_austrncpy_nid_postfix(chars, terminated, 6);
    Require(std::memcmp(chars, "Ab_\0XXXX", 8) == 0, "bounded copy stops at the terminator before the guard page");
    std::memset(chars, 'X', sizeof(chars));
    u_UCharsToChars_nid_postfix(terminated, chars, 4);
    Require(std::memcmp(chars, "Ab_\0XXXX", 8) == 0, "narrowing reads up to the guard page");
    std::memset(chars, 'X', sizeof(chars));
    u_austrcpy_nid_postfix(chars, straddling);
    Require(std::memcmp(chars, "xyz\0XXXX", 8) == 0, "odd-aligned copy reads units straddling a page");

    Place(guard - 2, u"a", 1);
    const auto* last = reinterpret_cast<const char16_t*>(guard - 2);
    std::memset(chars, 'X', sizeof(chars));
    Require(u_austrncpy_nid_postfix(chars, last, 1) == chars && std::memcmp(chars, "aXXXXXXX", 8) == 0, "bounded copy reads only the bound");
    std::memset(chars, 'X', sizeof(chars));
    RequiresRuntimeErrorKeepingErrno([&] { u_austrcpy_nid_postfix(chars, last); }, "unterminated copy source throws at the guard page");
    RequiresRuntimeErrorKeepingErrno([&] { u_austrncpy_nid_postfix(chars, last, 2); }, "bounded source past the guard page throws");
    RequiresRuntimeErrorKeepingErrno([&] { u_UCharsToChars_nid_postfix(last, chars, 2); }, "narrowing past the guard page throws");
    Require(chars[0] == 'X', "unreadable sources write nothing");

    std::memcpy(guard - 3, "Ab_", 3);
    const char* lastChars = guard - 3;
    for (auto& unit : units) unit = 0x5555;
    u_charsToUChars_nid_postfix(lastChars, units, 3);
    Require(units[0] == u'A' && units[1] == u'b' && units[2] == u'_' && units[3] == 0x5555, "widening reads up to the guard page");
    RequiresRuntimeErrorKeepingErrno([&] { u_charsToUChars_nid_postfix(lastChars, units, 4); }, "widening past the guard page throws");

    std::memset(guard - 4, 'X', 4);
    u_austrcpy_nid_postfix(guard - 4, u"abc");
    Require(std::memcmp(guard - 4, "abc", 4) == 0, "copy writes up to the guard page");
    std::memset(guard - 4, 'X', 4);
    RequiresRuntimeErrorKeepingErrno([&] { u_austrcpy_nid_postfix(guard - 1, u"a"); }, "copy destination running into the guard page throws");
    RequiresRuntimeErrorKeepingErrno([&] { u_austrncpy_nid_postfix(guard - 1, u"ab", 3); }, "bounded destination running into the guard page throws");
    RequiresRuntimeErrorKeepingErrno([&] { u_UCharsToChars_nid_postfix(u"ab", guard - 1, 2); }, "narrowing destination running into the guard page throws");
    RequiresRuntimeErrorKeepingErrno([&] { u_charsToUChars_nid_postfix("ab", reinterpret_cast<char16_t*>(guard - 2), 2); }, "widening destination running into the guard page throws");
    Require(std::memcmp(guard - 4, "XXXX", 4) == 0, "destinations running into the guard page stay unchanged");

    RequiresRuntimeErrorKeepingErrno([&] { u_austrcpy_nid_postfix(chars, unmappedUnits); }, "unreadable copy source throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_austrcpy_nid_postfix(unmappedOut, u"a"); }, "unwritable copy destination throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_austrcpy_nid_postfix(readOnly, u"a"); }, "read-only copy destination throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_austrncpy_nid_postfix(chars, unmappedUnits, 1); }, "unreadable bounded source throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_austrncpy_nid_postfix(readOnly, u"a", 1); }, "read-only bounded destination throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_UCharsToChars_nid_postfix(unmappedUnits, chars, 1); }, "unreadable narrowing source throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_UCharsToChars_nid_postfix(u"a", unmappedOut, 1); }, "unwritable narrowing destination throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_charsToUChars_nid_postfix(unmappedChars, units, 1); }, "unreadable widening source throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_charsToUChars_nid_postfix("a", unmappedUnitsOut, 1); }, "unwritable widening destination throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_charsToUChars_nid_postfix("a", reinterpret_cast<char16_t*>(readOnly), 1); }, "read-only widening destination throws and keeps errno");
    Require(readOnly[0] == 'R' && readOnly[1] == 'R', "failures leave read-only memory unchanged");

#ifdef _WIN32
    VirtualFree(base, 0, MEM_RELEASE);
#else
    munmap(base, pageSize * 4);
#endif
}

void LargeLengths() {
    constexpr std::size_t count = 10000;
    char chars[count];
    for (std::size_t index = 0; index < count; ++index) chars[index] = static_cast<char>(index);
    char16_t units[count + 1];
    units[count] = 0x5555;
    u_charsToUChars_nid_postfix(chars, units, count);
    for (std::size_t index = 0; index < count; ++index) Require(units[index] == static_cast<std::uint8_t>(chars[index]), "widening spans several chunks");
    Require(units[count] == 0x5555, "widening across chunks stays within the length");
    char narrowed[count + 1];
    narrowed[count] = 'X';
    for (std::size_t index = 0; index < count; ++index) units[index] = static_cast<char16_t>(u'a' + index % 26);
    u_UCharsToChars_nid_postfix(units, narrowed, count);
    for (std::size_t index = 0; index < count; ++index) Require(narrowed[index] == static_cast<char>('a' + index % 26), "narrowing spans several chunks");
    Require(narrowed[count] == 'X', "narrowing across chunks stays within the length");
    units[count] = 0;
    char copied[count + 1];
    Require(u_austrcpy_nid_postfix(copied, units) == copied && std::memcmp(copied, narrowed, count) == 0 && copied[count] == 0, "terminated copy spans several chunks");
}

}

int main() {
    GuestPointers();
    LargeLengths();
}
