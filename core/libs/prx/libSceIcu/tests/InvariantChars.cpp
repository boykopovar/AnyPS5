#include "prx/libSceIcu/libSceIcu.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu invariant chars: " + message);
}

char Byte(std::uint8_t value) { return static_cast<char>(value); }

template <typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct GuardedPages {
    char* begin = nullptr;
    std::size_t size = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        begin = static_cast<char*>(VirtualAlloc(nullptr, size * 3, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(begin != nullptr, "VirtualAlloc failed");
        DWORD previous = 0;
        Require(VirtualProtect(begin + size * 2, size, PAGE_NOACCESS, &previous) != 0, "VirtualProtect failed");
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, size * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "mmap failed");
        begin = static_cast<char*>(mapped);
        Require(mprotect(begin + size * 2, size, PROT_NONE) == 0, "mprotect failed");
#endif
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(begin, 0, MEM_RELEASE);
#else
        munmap(begin, size * 3);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    char* Guard() const { return begin + size * 2; }

    char* BeforeGuard(const void* bytes, std::size_t count) const {
        char* placed = Guard() - count;
        std::memcpy(placed, bytes, count);
        return placed;
    }
};

const char16_t* Units(const char* bytes) { return reinterpret_cast<const char16_t*>(bytes); }

void UnreadableInputs() {
    const auto* bytes = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    const char16_t* units = Units(bytes);
    errno = 71;
    Require(Throws([&] { uprv_stricmp_nid_postfix(bytes, "a"); }), "uprv_stricmp read an unreadable first string");
    Require(Throws([&] { uprv_stricmp_nid_postfix("a", bytes); }), "uprv_stricmp read an unreadable second string");
    Require(Throws([&] { uprv_strnicmp_nid_postfix(bytes, "a", 1); }), "uprv_strnicmp read an unreadable string");
    Require(Throws([&] { uprv_isInvariantString_nid_postfix(bytes, -1); }),
        "uprv_isInvariantString read an unreadable terminated string");
    Require(Throws([&] { uprv_isInvariantString_nid_postfix(bytes, 1); }),
        "uprv_isInvariantString read an unreadable counted string");
    Require(Throws([&] { uprv_isInvariantUString_nid_postfix(units, -1); }),
        "uprv_isInvariantUString read an unreadable terminated string");
    Require(Throws([&] { uprv_isInvariantUString_nid_postfix(units, 1); }),
        "uprv_isInvariantUString read an unreadable counted string");
    Require(errno == 71, "an unreadable string changed errno");
    Require(uprv_strnicmp_nid_postfix(bytes, bytes, 0) == 0, "uprv_strnicmp read a string with n of zero");
    Require(uprv_isInvariantString_nid_postfix(bytes, 0) == 1, "uprv_isInvariantString read an empty string");
    Require(uprv_isInvariantUString_nid_postfix(units, 0) == 1, "uprv_isInvariantUString read an empty string");
}

void StringsBeforeAGuard() {
    GuardedPages pages;
    const char* tail = pages.BeforeGuard("abc", 3);
    Require(uprv_stricmp_nid_postfix(tail, "abD") == 'c' - 'd', "uprv_stricmp read past the first difference");
    Require(uprv_stricmp_nid_postfix("ab", tail) == -1, "uprv_stricmp read past the shorter string");
    Require(Throws([&] { uprv_stricmp_nid_postfix(tail, "ABC"); }), "uprv_stricmp read across the guard");
    Require(uprv_strnicmp_nid_postfix(tail, "ABCD", 3) == 0, "uprv_strnicmp read past n");
    Require(Throws([&] { uprv_strnicmp_nid_postfix(tail, "ABC", 4); }), "uprv_strnicmp read across the guard");
    Require(uprv_isInvariantString_nid_postfix(tail, 3) == 1, "uprv_isInvariantString read past length");
    Require(Throws([&] { uprv_isInvariantString_nid_postfix(tail, -1); }),
        "uprv_isInvariantString read across the guard");

    const char* variant = pages.BeforeGuard("a@c", 3);
    Require(uprv_isInvariantString_nid_postfix(variant, -1) == 0,
        "uprv_isInvariantString read past a variant character");
    const char* terminated = pages.BeforeGuard("Ab", 3);
    Require(uprv_stricmp_nid_postfix(terminated, "aB") == 0, "uprv_stricmp missed a terminator before the guard");
    Require(uprv_isInvariantString_nid_postfix(terminated, -1) == 1,
        "uprv_isInvariantString missed a terminator before the guard");

    const char16_t* units = Units(pages.BeforeGuard(u"ab", 4));
    Require(uprv_isInvariantUString_nid_postfix(units, 2) == 1, "uprv_isInvariantUString read past length");
    Require(Throws([&] { uprv_isInvariantUString_nid_postfix(units, -1); }),
        "uprv_isInvariantUString read across the guard");
    const char16_t* variantUnits = Units(pages.BeforeGuard(u"a~", 4));
    Require(uprv_isInvariantUString_nid_postfix(variantUnits, -1) == 0,
        "uprv_isInvariantUString read past a variant character");

    char* straddling = pages.Guard() - 3;
    std::memcpy(straddling, u"a", 2);
    Require(uprv_isInvariantUString_nid_postfix(Units(straddling), 1) == 1,
        "uprv_isInvariantUString rejected a misaligned unit");
    Require(Throws([&] { uprv_isInvariantUString_nid_postfix(Units(straddling), 2); }),
        "uprv_isInvariantUString read a unit straddling the guard");
}

void StringsAcrossAPage(std::size_t offset) {
    GuardedPages pages;
    char* across = pages.begin + pages.size - offset;
    const std::string upper(offset * 2, 'A');
    std::string lower(offset * 2, 'a');
    std::memcpy(across, upper.c_str(), upper.size() + 1);
    Require(uprv_stricmp_nid_postfix(across, lower.c_str()) == 0, "uprv_stricmp stopped at a page boundary");
    Require(uprv_strnicmp_nid_postfix(across, lower.c_str(), static_cast<std::uint32_t>(upper.size() + 7)) == 0,
        "uprv_strnicmp stopped at a page boundary");
    lower.back() = 'b';
    Require(uprv_stricmp_nid_postfix(across, lower.c_str()) == 'a' - 'b',
        "uprv_stricmp missed a difference after a page boundary");
    Require(uprv_stricmp_nid_postfix(lower.c_str(), across) == 'b' - 'a',
        "uprv_stricmp missed a difference after a page boundary in the first string");
    Require(uprv_strnicmp_nid_postfix(across, lower.c_str(), static_cast<std::uint32_t>(upper.size() - 1)) == 0,
        "uprv_strnicmp compared past n after a page boundary");
    Require(uprv_isInvariantString_nid_postfix(across, -1) == 1,
        "uprv_isInvariantString stopped at a page boundary");
    across[upper.size() - 1] = '@';
    Require(uprv_isInvariantString_nid_postfix(across, -1) == 0,
        "uprv_isInvariantString missed a variant character after a page boundary");
    Require(uprv_isInvariantString_nid_postfix(across, static_cast<std::int32_t>(upper.size() - 1)) == 1,
        "uprv_isInvariantString read past length after a page boundary");

    char* unitsAcross = pages.begin + pages.size - offset - 1;
    std::memcpy(unitsAcross, u"abcd", 10);
    Require(uprv_isInvariantUString_nid_postfix(Units(unitsAcross), -1) == 1,
        "uprv_isInvariantUString stopped at a page boundary");
    std::memcpy(unitsAcross, u"abc~", 10);
    Require(uprv_isInvariantUString_nid_postfix(Units(unitsAcross), -1) == 0,
        "uprv_isInvariantUString missed a variant character after a page boundary");
    Require(uprv_isInvariantUString_nid_postfix(Units(unitsAcross), 3) == 1,
        "uprv_isInvariantUString read past length after a page boundary");
}

}

int main() {
    UnreadableInputs();
    StringsBeforeAGuard();
    StringsAcrossAPage(2);
    StringsAcrossAPage(50);

    Require(u_asciiToUpper_nid_postfix(u'a') == u'A', "u_asciiToUpper did not raise a");
    Require(u_asciiToUpper_nid_postfix(u'z') == u'Z', "u_asciiToUpper did not raise z");
    Require(u_asciiToUpper_nid_postfix(u'`') == u'`', "u_asciiToUpper changed the character before a");
    Require(u_asciiToUpper_nid_postfix(u'{') == u'{', "u_asciiToUpper changed the character after z");
    Require(u_asciiToUpper_nid_postfix(u'Q') == u'Q', "u_asciiToUpper changed an uppercase letter");
    Require(u_asciiToUpper_nid_postfix(u'é') == u'é', "u_asciiToUpper changed a non-ASCII letter");
    Require(u_asciiToUpper_nid_postfix(static_cast<char16_t>(0xff41)) == static_cast<char16_t>(0xff41),
        "u_asciiToUpper changed a fullwidth letter");

    Require(uprv_toupper_nid_postfix('a') == 'A', "uprv_toupper did not raise a");
    Require(uprv_toupper_nid_postfix('z') == 'Z', "uprv_toupper did not raise z");
    Require(uprv_toupper_nid_postfix('`') == '`', "uprv_toupper changed the character before a");
    Require(uprv_toupper_nid_postfix('{') == '{', "uprv_toupper changed the character after z");
    Require(uprv_toupper_nid_postfix(Byte(0xe9)) == Byte(0xe9), "uprv_toupper changed a high byte");

    Require(uprv_asciitolower_nid_postfix('A') == 'a', "uprv_asciitolower did not lower A");
    Require(uprv_asciitolower_nid_postfix('Z') == 'z', "uprv_asciitolower did not lower Z");
    Require(uprv_asciitolower_nid_postfix('@') == '@', "uprv_asciitolower changed the character before A");
    Require(uprv_asciitolower_nid_postfix('[') == '[', "uprv_asciitolower changed the character after Z");
    Require(uprv_asciitolower_nid_postfix(Byte(0xc1)) == Byte(0xc1), "uprv_asciitolower changed a high byte");

    Require(uprv_ebcdictolower_nid_postfix(Byte(0xc1)) == Byte(0x81), "uprv_ebcdictolower did not lower EBCDIC A");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xc9)) == Byte(0x89), "uprv_ebcdictolower did not lower EBCDIC I");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xd1)) == Byte(0x91), "uprv_ebcdictolower did not lower EBCDIC J");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xd9)) == Byte(0x99), "uprv_ebcdictolower did not lower EBCDIC R");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xe2)) == Byte(0xa2), "uprv_ebcdictolower did not lower EBCDIC S");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xe9)) == Byte(0xa9), "uprv_ebcdictolower did not lower EBCDIC Z");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xc0)) == Byte(0xc0), "uprv_ebcdictolower changed 0xc0");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xca)) == Byte(0xca), "uprv_ebcdictolower changed 0xca");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xe1)) == Byte(0xe1), "uprv_ebcdictolower changed 0xe1");
    Require(uprv_ebcdictolower_nid_postfix('A') == 'A', "uprv_ebcdictolower changed ASCII A");

    Require(uprv_stricmp_nid_postfix("Hello", "hELLO") == 0, "uprv_stricmp did not ignore case");
    Require(uprv_stricmp_nid_postfix("A", "c") == 'a' - 'c', "uprv_stricmp did not return the lowercase difference");
    Require(uprv_stricmp_nid_postfix("[", "a") == '[' - 'a', "uprv_stricmp lowered a character outside A-Z");
    Require(uprv_stricmp_nid_postfix("ab", "a") == 1, "uprv_stricmp did not order the longer string after");
    Require(uprv_stricmp_nid_postfix("a", "ab") == -1, "uprv_stricmp did not order the shorter string before");
    Require(uprv_stricmp_nid_postfix(nullptr, nullptr) == 0, "uprv_stricmp did not treat two nulls as equal");
    Require(uprv_stricmp_nid_postfix(nullptr, "a") == -1, "uprv_stricmp did not order null first");
    Require(uprv_stricmp_nid_postfix("a", nullptr) == 1, "uprv_stricmp did not order null first");

    Require(uprv_strnicmp_nid_postfix("HELLOx", "helloy", 5) == 0, "uprv_strnicmp compared past n");
    Require(uprv_strnicmp_nid_postfix("HELLOx", "helloy", 6) == 'x' - 'y', "uprv_strnicmp stopped before n");
    Require(uprv_strnicmp_nid_postfix("a", "b", 0) == 0, "uprv_strnicmp compared with n of zero");
    Require(uprv_strnicmp_nid_postfix("ab", "a", 2) == 1, "uprv_strnicmp did not order the longer string after");
    Require(uprv_strnicmp_nid_postfix("Ab", "aB", 100) == 0, "uprv_strnicmp did not stop at the terminator");
    Require(uprv_strnicmp_nid_postfix(nullptr, "a", 1) == -1, "uprv_strnicmp did not order null first");

    Require(uprv_isInvariantString_nid_postfix("Hello, world.", -1) == 1, "uprv_isInvariantString rejected invariant text");
    Require(uprv_isInvariantString_nid_postfix("a@b", -1) == 0, "uprv_isInvariantString accepted @");
    Require(uprv_isInvariantString_nid_postfix("Hi!", -1) == 0, "uprv_isInvariantString accepted !");
    Require(uprv_isInvariantString_nid_postfix("abc@", 3) == 1, "uprv_isInvariantString read past length");
    const char high[] = {'a', Byte(0xc3), Byte(0xa9), 0};
    Require(uprv_isInvariantString_nid_postfix(high, -1) == 0, "uprv_isInvariantString accepted a high byte");
    Require(uprv_isInvariantString_nid_postfix(nullptr, 0) == 1, "uprv_isInvariantString rejected an empty null string");
    Require(Throws([] { uprv_isInvariantString_nid_postfix(nullptr, -1); }),
        "uprv_isInvariantString accepted a null string");

    Require(uprv_isInvariantUString_nid_postfix(u"Hello, world.", -1) == 1,
        "uprv_isInvariantUString rejected invariant text");
    Require(uprv_isInvariantUString_nid_postfix(u"a~", -1) == 0, "uprv_isInvariantUString accepted ~");
    Require(uprv_isInvariantUString_nid_postfix(u"café", -1) == 0, "uprv_isInvariantUString accepted a non-ASCII letter");
    Require(uprv_isInvariantUString_nid_postfix(u"abc~", 3) == 1, "uprv_isInvariantUString read past length");
    Require(uprv_isInvariantUString_nid_postfix(nullptr, 0) == 1, "uprv_isInvariantUString rejected an empty null string");
    Require(Throws([] { uprv_isInvariantUString_nid_postfix(nullptr, 2); }),
        "uprv_isInvariantUString accepted a null string");
    return 0;
}
