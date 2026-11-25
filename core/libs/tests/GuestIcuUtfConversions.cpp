#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
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

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU UTF conversion: %s\n", message);
    std::abort();
}

std::string Utf8(std::uint32_t value) {
    if (value <= 0x7f) return {static_cast<char>(value)};
    if (value <= 0x7ff) return {static_cast<char>(0xc0 + value / 64), static_cast<char>(0x80 + value % 64)};
    if (value <= 0xffff) return {static_cast<char>(0xe0 + value / 4096), static_cast<char>(0x80 + value / 64 % 64), static_cast<char>(0x80 + value % 64)};
    return {static_cast<char>(0xf0 + value / 262144), static_cast<char>(0x80 + value / 4096 % 64), static_cast<char>(0x80 + value / 64 % 64), static_cast<char>(0x80 + value % 64)};
}

std::u16string Utf16(std::uint32_t value) {
    if (value <= 0xffff) return {static_cast<char16_t>(value)};
    value -= 0x10000;
    return {static_cast<char16_t>(0xd800 + value / 1024), static_cast<char16_t>(0xdc00 + value % 1024)};
}

void Scalars() {
    for (std::uint32_t value = 0; value <= 0x10ffff; ++value) {
        if (value >= 0xd800 && value <= 0xdfff) continue;
        const auto bytes = Utf8(value);
        const auto units = Utf16(value);
        std::array<char16_t, 4> wide{0x5555, 0x5555, 0x5555, 0x5555};
        std::array<char, 6> narrow{'X', 'X', 'X', 'X', 'X', 'X'};
        std::int32_t length = -1, error = 0;
        Require(u_strFromUTF8_nid_postfix(wide.data(), 3, &length, bytes.data(), bytes.size(), &error) == wide.data(), "scalar UTF-8 return");
        Require(error == 0 && length == static_cast<std::int32_t>(units.size()) && std::memcmp(wide.data(), units.data(), units.size() * 2) == 0 && wide[units.size()] == 0 && wide[3] == 0x5555, "scalar UTF-8 decode");
        Require(u_strToUTF8_nid_postfix(narrow.data(), 5, &length, units.data(), units.size(), &error) == narrow.data(), "scalar UTF-16 return");
        Require(error == 0 && length == static_cast<std::int32_t>(bytes.size()) && std::memcmp(narrow.data(), bytes.data(), bytes.size()) == 0 && narrow[bytes.size()] == 0 && narrow[5] == 'X', "scalar UTF-16 encode");
        wide.fill(0x5555);
        Require(u_strFromUTF8Lenient_nid_postfix(wide.data(), 4, &length, bytes.data(), bytes.size(), &error) == wide.data(), "scalar lenient return");
        Require(error == 0 && length == static_cast<std::int32_t>(units.size()) && std::memcmp(wide.data(), units.data(), units.size() * 2) == 0 && wide[units.size()] == 0, "scalar lenient decode");
    }
}

void Capacity() {
    const std::string bytes = std::string("A\0", 2) + Utf8(0x1f642) + Utf8(0x20ac) + "Z";
    const std::u16string units = std::u16string(u"A\0", 2) + Utf16(0x1f642) + u"\u20acZ";
    for (std::int32_t capacity = 0; capacity <= 12; ++capacity) {
        std::array<char16_t, 16> wide;
        wide.fill(0x5555);
        std::int32_t length = -1, error = 0;
        Require(u_strFromUTF8_nid_postfix(wide.data(), capacity, &length, bytes.data(), bytes.size(), &error) == wide.data(), "capacity UTF-8 return");
        Require(length == static_cast<std::int32_t>(units.size()) && error == (capacity < length ? 15 : capacity == length ? -124 : 0), "capacity UTF-8 status");
        for (std::int32_t index = 0; index < 16; ++index) Require(wide[index] == (index < capacity && index < length ? units[index] : index == length && index < capacity ? 0 : 0x5555), "capacity UTF-8 prefix or canary");
        std::array<char, 16> narrow;
        narrow.fill('X');
        error = 0;
        Require(u_strToUTF8_nid_postfix(narrow.data(), capacity, &length, units.data(), units.size(), &error) == narrow.data(), "capacity UTF-16 return");
        Require(length == static_cast<std::int32_t>(bytes.size()) && error == (capacity < length ? 15 : capacity == length ? -124 : 0), "capacity UTF-16 status");
        const auto prefix = capacity < 2 ? capacity : capacity < 6 ? 2 : capacity < 9 ? 6 : capacity < 10 ? 9 : 10;
        for (std::int32_t index = 0; index < 16; ++index) Require(narrow[index] == (index < prefix ? bytes[index] : index == length && index < capacity ? 0 : 'X'), "UTF-8 complete-sequence prefix or canary");
        wide.fill(0x5555);
        error = 0;
        auto* result = u_strFromUTF8Lenient_nid_postfix(wide.data(), capacity, &length, bytes.data(), bytes.size(), &error);
        if (capacity < static_cast<std::int32_t>(bytes.size())) Require(!result && error == 15 && length == static_cast<std::int32_t>(bytes.size()) && wide[0] == 0x5555, "lenient capacity fast rejection");
        else Require(result == wide.data() && error == 0 && length == static_cast<std::int32_t>(units.size()) && std::memcmp(wide.data(), units.data(), units.size() * 2) == 0, "lenient capacity conversion");
    }
    std::int32_t error = 0, length = -1;
    Require(!u_strFromUTF8_nid_postfix(nullptr, 0, &length, bytes.data(), bytes.size(), &error) && error == 15 && length == static_cast<std::int32_t>(units.size()), "UTF-8 null preflight");
    error = 0;
    Require(!u_strToUTF8_nid_postfix(nullptr, 0, &length, units.data(), units.size(), &error) && error == 15 && length == static_cast<std::int32_t>(bytes.size()), "UTF-16 null preflight");
    std::array<char16_t, 8> wide{};
    error = 0;
    u_strFromUTF8_nid_postfix(wide.data(), 8, &length, bytes.c_str(), -1, &error);
    Require(error == 0 && length == 1 && wide[0] == u'A' && wide[1] == 0, "UTF-8 NUL length");
    std::array<char, 8> narrow{};
    u_strToUTF8_nid_postfix(narrow.data(), 8, &length, units.c_str(), -1, &error);
    Require(error == 0 && length == 1 && narrow[0] == 'A' && narrow[1] == 0, "UTF-16 NUL length");
}

void Malformed() {
    const std::vector<std::pair<std::string, std::u16string>> cases{
        {std::string("\x80", 1), u"\ufffd"}, {std::string("\xc0\x80", 2), u"\ufffd\ufffd"},
        {std::string("\xe0\x80\x80", 3), u"\ufffd\ufffd\ufffd"}, {std::string("\xed\xa0\x80", 3), u"\ufffd\ufffd\ufffd"},
        {std::string("\xf4\x90\x80\x80", 4), u"\ufffd\ufffd\ufffd\ufffd"},
        {std::string("\xe2\x82", 2), u"\ufffd"}, {std::string("\xf0\x90\x80", 3), u"\ufffd"},
        {std::string("\xe2\x82" "A", 3), u"\ufffdA"}
    };
    for (const auto& [input, expected] : cases) {
        for (const std::int32_t sourceLength : {-1, static_cast<std::int32_t>(input.size())}) {
            std::array<char16_t, 20> output;
            output.fill(0x5555);
            std::int32_t length = 77, error = 0, substitutions = 99;
            Require(!u_strFromUTF8_nid_postfix(output.data(), 20, &length, input.c_str(), sourceLength, &error) && error == 10 && length == 77 && output[0] == 0x5555, "strict malformed UTF-8");
            error = 0;
            Require(u_strFromUTF8WithSub_nid_postfix(output.data(), 20, &length, input.c_str(), sourceLength, 0xfffd, &substitutions, &error) == output.data(), "substitution return");
            Require(error == 0 && length == static_cast<std::int32_t>(expected.size()) && std::memcmp(output.data(), expected.data(), expected.size() * 2) == 0, "maximal-subpart replacement");
            std::int32_t expectedCount = 0;
            for (const auto value : expected) expectedCount += value == 0xfffd;
            Require(substitutions == expectedCount && output[length] == 0, "substitution count and terminator");
            error = 0;
            u_strFromUTF8WithSub_nid_postfix(output.data(), 20, &length, input.c_str(), sourceLength, 0x1f642, &substitutions, &error);
            Require(error == 0 && length == static_cast<std::int32_t>(expected.size()) + expectedCount && substitutions == expectedCount, "supplementary substitution geometry");
        }
    }
    const std::u16string input{u'A', 0xd800, u'B', 0xdc00, 0xd800, 0xdc00};
    const auto expected = std::string("A?") + "B?" + Utf8(0x10000);
    std::array<char, 32> output{};
    std::int32_t length = 77, substitutions = 99, error = 0;
    Require(!u_strToUTF8_nid_postfix(output.data(), 32, &length, input.data(), input.size(), &error) && error == 10 && length == 77 && output[0] == 'A' && output[1] == 0, "strict isolated surrogate prefix");
    error = 0;
    Require(u_strToUTF8WithSub_nid_postfix(output.data(), 32, &length, input.data(), input.size(), '?', &substitutions, &error) == output.data(), "surrogate substitution return");
    Require(error == 0 && substitutions == 2 && length == static_cast<std::int32_t>(expected.size()) && std::memcmp(output.data(), expected.data(), expected.size()) == 0, "surrogate substitutions and valid pair");
    const std::string overlong("\xc0\x80", 2);
    std::array<char16_t, 4> wide{0x5555, 0x5555, 0x5555, 0x5555};
    error = 0;
    u_strFromUTF8Lenient_nid_postfix(wide.data(), 4, &length, overlong.data(), overlong.size(), &error);
    Require(error == 0 && length == 1 && wide[0] == 0 && wide[1] == 0, "lenient overlong form");
    error = 0;
    u_strFromUTF8Lenient_nid_postfix(wide.data(), 4, &length, "\xf0\x90", -1, &error);
    Require(error == 0 && length == 1 && wide[0] == 0xfffd && wide[1] == 0, "lenient truncated sequence");
}

void Status() {
    std::array<char16_t, 8> output;
    output.fill(0x5555);
    std::int32_t length = 77, substitutions = 99, error = 10;
    Require(!u_strFromUTF8WithSub_nid_postfix(output.data(), 8, &length, "a", 1, '?', &substitutions, &error) && length == 77 && substitutions == 99 && output[0] == 0x5555 && error == 10, "latched failure output preservation");
    for (const std::int32_t warning : {-124, -128}) {
        error = warning;
        Require(u_strFromUTF8_nid_postfix(output.data(), 8, &length, "a", -1, &error) == output.data() && error == (warning == -124 ? 0 : warning) && length == 1, "warning propagation");
    }
    for (const std::int32_t substitution : {0xd800, 0xdfff, 0x110000}) {
        error = 0; length = 77; substitutions = 99;
        Require(!u_strFromUTF8WithSub_nid_postfix(output.data(), 8, &length, "a", 1, substitution, &substitutions, &error) && error == 1 && length == 77 && substitutions == 99, "invalid substitution argument");
    }
    error = 0;
    Require(!u_strFromUTF8_nid_postfix(nullptr, 1, &length, "a", 1, &error) && error == 1, "null positive destination");
    error = 0;
    Require(!u_strFromUTF8_nid_postfix(output.data(), -1, &length, "a", 1, &error) && error == 1, "negative capacity");
    error = 0;
    Require(!u_strFromUTF8_nid_postfix(output.data(), 8, &length, nullptr, 1, &error) && error == 1, "null positive source");
    error = 0;
    Require(u_strFromUTF8_nid_postfix(output.data(), 8, nullptr, nullptr, 0, &error) == output.data() && error == 0 && output[0] == 0, "null empty source");
}

void SubstitutionGeometry() {
    std::array<char16_t, 8> wide;
    wide.fill(0x5555);
    std::int32_t length = 77, substitutions = 77, error = 0;
    Require(u_strFromUTF8WithSub_nid_postfix(wide.data(), 1, &length, "\x80", 1, 0x1f642, &substitutions, &error) == wide.data(), "supplementary substitution overflow return");
    Require(error == 15 && length == 2 && substitutions == 1 && wide[0] == 0xd83d && wide[1] == 0x5555, "supplementary substitution overflow");
    wide.fill(0x5555);
    error = 0;
    Require(u_strFromUTF8WithSub_nid_postfix(wide.data(), 2, &length, "\x80", 1, 0x1f642, &substitutions, &error) == wide.data(), "supplementary substitution terminator return");
    Require(error == -124 && length == 2 && substitutions == 1 && wide[0] == 0xd83d && wide[1] == 0xde42, "supplementary substitution fills the buffer exactly");
    wide.fill(0x5555);
    length = 77;
    substitutions = 77;
    error = 0;
    Require(u_strFromUTF8WithSub_nid_postfix(wide.data(), 4, &length, "\x80" "a", 2, 0, &substitutions, &error) == wide.data(), "nul substitution return");
    Require(error == 0 && length == 2 && substitutions == 1 && wide[0] == 0 && wide[1] == u'a' && wide[2] == 0 && wide[3] == 0x5555, "nul substitution");
    const char16_t lone[]{0xd800};
    std::array<char, 8> narrow;
    narrow.fill('X');
    length = 77;
    substitutions = 77;
    error = 0;
    Require(u_strToUTF8WithSub_nid_postfix(narrow.data(), 3, &length, lone, 1, 0x1f642, &substitutions, &error) == narrow.data(), "encode supplementary overflow return");
    Require(error == 15 && length == 4 && substitutions == 1 && narrow[0] == 'X' && narrow[1] == 'X', "encode supplementary overflow");
    error = 0;
    Require(u_strToUTF8WithSub_nid_postfix(narrow.data(), 4, &length, lone, 1, 0x1f642, &substitutions, &error) == narrow.data(), "encode supplementary terminator return");
    Require(error == -124 && length == 4 && substitutions == 1 && static_cast<unsigned char>(narrow[0]) == 0xf0 && narrow[4] == 'X', "encode supplementary fills the buffer exactly");
}

void Empty() {
    std::array<char16_t, 4> wide{0x5555, 0x5555, 0x5555, 0x5555};
    std::int32_t length = 77, error = 0;
    Require(u_strFromUTF8_nid_postfix(wide.data(), 0, &length, "", 0, &error) == wide.data() && error == -124 && length == 0 && wide[0] == 0x5555, "empty result without room");
    error = 0;
    Require(u_strFromUTF8_nid_postfix(wide.data(), 1, &length, "", -1, &error) == wide.data() && error == 0 && length == 0 && wide[0] == 0 && wide[1] == 0x5555, "empty result with room");
    std::array<char, 4> narrow{'X', 'X', 'X', 'X'};
    length = 77;
    error = 0;
    Require(u_strToUTF8_nid_postfix(narrow.data(), 0, &length, u"", 0, &error) == narrow.data() && error == -124 && length == 0 && narrow[0] == 'X', "empty encode without room");
}

void LongStrings() {
    std::u16string units;
    std::string bytes;
    for (int index = 0; index < 3000; ++index) {
        units += u"a";
        units += u"\u20ac";
        units += u"\U0001f642";
        bytes += "a";
        bytes += Utf8(0x20ac);
        bytes += Utf8(0x1f642);
    }
    std::vector<char16_t> wide(units.size() + 1, 0x5555);
    std::int32_t length = -1, error = 0;
    Require(u_strFromUTF8_nid_postfix(wide.data(), static_cast<std::int32_t>(wide.size()), &length, bytes.c_str(), -1, &error) == wide.data(), "long UTF-8 decode return");
    Require(error == 0 && length == static_cast<std::int32_t>(units.size()) && std::memcmp(wide.data(), units.data(), units.size() * 2) == 0 && wide[units.size()] == 0, "long UTF-8 decode");
    error = 0;
    Require(u_strFromUTF8_nid_postfix(wide.data(), static_cast<std::int32_t>(wide.size()), &length, bytes.data(), static_cast<std::int32_t>(bytes.size()), &error) == wide.data(), "long counted UTF-8 decode return");
    Require(error == 0 && length == static_cast<std::int32_t>(units.size()) && std::memcmp(wide.data(), units.data(), units.size() * 2) == 0, "long counted UTF-8 decode");
    std::vector<char> narrow(bytes.size() + 1, 'X');
    error = 0;
    Require(u_strToUTF8_nid_postfix(narrow.data(), static_cast<std::int32_t>(narrow.size()), &length, units.c_str(), -1, &error) == narrow.data(), "long UTF-16 encode return");
    Require(error == 0 && length == static_cast<std::int32_t>(bytes.size()) && std::memcmp(narrow.data(), bytes.data(), bytes.size()) == 0 && narrow[bytes.size()] == 0, "long UTF-16 encode");
    error = 0;
    Require(u_strToUTF8_nid_postfix(narrow.data(), static_cast<std::int32_t>(bytes.size()) - 1, &length, units.data(), static_cast<std::int32_t>(units.size()), &error) == narrow.data(), "long UTF-16 overflow return");
    Require(error == 15 && length == static_cast<std::int32_t>(bytes.size()), "long UTF-16 overflow");
}

template<class TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

class GuardedPage {
public:
    explicit GuardedPage(std::size_t count = 1) : pages(count) {
#ifdef _WIN32
        SYSTEM_INFO info {};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        base = static_cast<char*>(VirtualAlloc(nullptr, size * (pages + 1), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(base != nullptr, "guard page allocation");
        DWORD previous {};
        Require(VirtualProtect(End(), size, PAGE_NOACCESS, &previous) != 0, "guard page protection");
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, size * (pages + 1), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "guard page allocation");
        base = static_cast<char*>(mapped);
        Require(mprotect(End(), size, PROT_NONE) == 0, "guard page protection");
#endif
    }
    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size * (pages + 1));
#endif
    }
    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

    char* SecondPage() const { return base + size; }
    char* End() const { return base + size * pages; }
    void ReadOnly() const {
#ifdef _WIN32
        DWORD previous {};
        Require(VirtualProtect(base, size, PAGE_READONLY, &previous) != 0, "read-only page");
#else
        Require(mprotect(base, size, PROT_READ) == 0, "read-only page");
#endif
    }

private:
    std::size_t pages;
    char* base = nullptr;
    std::size_t size = 0;
};

void LenientGuestPointers() {
    constexpr auto huge = std::numeric_limits<std::int32_t>::max();
    std::array<char16_t, 8> wide;
    wide.fill(0x5555);
    std::int32_t length = 77, error = 0;
    auto* unmappedStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    auto* unmappedLength = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    const auto* unmappedSource = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    auto* unmappedDestination = reinterpret_cast<char16_t*>(std::uintptr_t{0x10});
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, "a", 1, unmappedStatus); }), "lenient unreadable status");
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, "a", 1, nullptr); }), "lenient null status");
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, unmappedLength, "a", 1, &error); }), "lenient unwritable length");
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, unmappedSource, 3, &error); }), "lenient unreadable counted source");
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, unmappedSource, -1, &error); }), "lenient unreadable terminated source");
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(unmappedDestination, 8, &length, "a", 1, &error); }), "lenient unwritable destination");
    Require(length == 77 && error == 0 && wide[0] == 0x5555, "lenient failures leave outputs unchanged");
    error = 10;
    Require(!u_strFromUTF8Lenient_nid_postfix(unmappedDestination, 8, unmappedLength, unmappedSource, 3, &error) && error == 10, "lenient latched failure reads nothing else");
    error = 0;
    Require(!u_strFromUTF8Lenient_nid_postfix(wide.data(), 2, &length, unmappedSource, huge, &error) && error == 15 && length == huge, "lenient oversized count fast rejection");

    GuardedPage page;
    auto* tail = page.End() - 4;
    std::memcpy(tail, "\xe2\x82\xac" "A", 4);
    error = 0;
    length = 77;
    Require(u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, tail, 4, &error) == wide.data() && error == 0 && length == 2 &&
        wide[0] == 0x20ac && wide[1] == u'A' && wide[2] == 0, "lenient source ending at a guard page");
    wide.fill(0x5555);
    length = 77;
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), huge, &length, tail, huge, &error); }), "lenient oversized count reaching a guard page");
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, tail, -1, &error); }), "lenient unterminated source reaching a guard page");
    Require(length == 77 && error == 0 && wide[0] == 0x5555, "lenient guard page failures leave outputs unchanged");
    auto* last = reinterpret_cast<char16_t*>(page.End()) - 2;
    Require(u_strFromUTF8Lenient_nid_postfix(last, 2, &length, "ab", 2, &error) == last && error == -124 && length == 2 && last[0] == u'a' && last[1] == u'b',
        "lenient destination ending at a guard page");
    error = 0;
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(last, huge, &length, "abc", 3, &error); }), "lenient destination crossing a guard page");
    Require(error == 0, "lenient destination failure leaves the status unchanged");

    GuardedPage readOnly;
    auto* frozen = reinterpret_cast<char16_t*>(readOnly.End()) - 4;
    auto* frozenStatus = reinterpret_cast<std::int32_t*>(readOnly.End() - 32);
    auto* frozenLength = reinterpret_cast<std::int32_t*>(readOnly.End() - 64);
    *frozenStatus = 0;
    readOnly.ReadOnly();
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(frozen, 4, &length, "a", 1, &error); }), "lenient read-only destination");
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, "a", 1, frozenStatus); }), "lenient read-only status");
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, frozenLength, "a", 1, &error); }), "lenient read-only length");
    Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), 0, frozenLength, "a", 1, &error); }), "lenient read-only preflight length");
}

void LenientPageBoundaries() {
    constexpr std::array<std::uint32_t, 3> scalars{0xe9, 0x20ac, 0x1f600};
    GuardedPage accessible(2);
    GuardedPage guarded;
    for (const auto scalar : scalars) {
        const auto encoded = Utf8(scalar);
        const auto expected = Utf16(scalar);
        const auto width = static_cast<std::int32_t>(encoded.size());
        const auto units = static_cast<std::int32_t>(expected.size());
        for (std::int32_t split = 1; split < width; ++split) {
            auto* source = accessible.SecondPage() - split;
            std::memcpy(source, encoded.data(), encoded.size());
            source[width] = 'A';
            source[width + 1] = 0;
            for (const std::int32_t sourceLength : {width + 1, -1}) {
                std::array<char16_t, 8> wide;
                wide.fill(0x5555);
                std::int32_t length = 77, error = 0;
                Require(u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, source, sourceLength, &error) == wide.data() && error == 0 &&
                    length == units + 1 && std::memcmp(wide.data(), expected.data(), expected.size() * 2) == 0 && wide[units] == u'A' &&
                    wide[units + 1] == 0 && wide[units + 2] == 0x5555, "lenient sequence crossing an accessible page boundary");
            }

            auto* tail = guarded.End() - split;
            std::memcpy(tail, encoded.data(), static_cast<std::size_t>(split));
            std::array<char16_t, 8> wide;
            wide.fill(0x5555);
            std::int32_t length = 77, error = 0;
            Require(u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, tail, split, &error) == wide.data() && error == 0 && length == 1 &&
                wide[0] == 0xfffd && wide[1] == 0 && wide[2] == 0x5555, "lenient truncated sequence ending at a guard page");
            wide.fill(0x5555);
            length = 77;
            Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, tail, width, &error); }), "lenient counted sequence crossing into a guard page");
            Require(Throws([&] { u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, tail, -1, &error); }), "lenient unterminated sequence crossing into a guard page");
            Require(length == 77 && error == 0 && wide[0] == 0x5555, "lenient guard page sequence failures leave outputs unchanged");
            auto* terminated = guarded.End() - split - 1;
            std::memcpy(terminated, encoded.data(), static_cast<std::size_t>(split));
            terminated[split] = 0;
            Require(u_strFromUTF8Lenient_nid_postfix(wide.data(), 8, &length, terminated, -1, &error) == wide.data() && error == 0 && length == 1 &&
                wide[0] == 0xfffd && wide[1] == 0 && wide[2] == 0x5555, "lenient terminated truncated sequence before a guard page");
        }
    }
}

void LenientChunks() {
    std::string bytes;
    std::u16string units;
    for (std::uint32_t index = 0; index < 0x900; ++index) {
        const std::uint32_t value = index % 3 == 0 ? 0x41 + index % 26 : index % 3 == 1 ? 0x20ac + index : 0x1f600 + index % 0x40;
        bytes += Utf8(value);
        units += Utf16(value);
    }
    const auto total = static_cast<std::int32_t>(units.size());
    const auto bounded = static_cast<std::int32_t>(bytes.size());
    std::vector<char16_t> wide(bytes.size() + 4, 0x5555);
    std::int32_t length = 0, error = 0;
    Require(u_strFromUTF8Lenient_nid_postfix(wide.data(), bounded, &length, bytes.data(), bounded, &error) == wide.data(), "lenient chunked return");
    Require(error == 0 && length == total && std::memcmp(wide.data(), units.data(), units.size() * 2) == 0 && wide[units.size()] == 0 && wide[units.size() + 1] == 0x5555,
        "lenient chunked conversion");
    std::fill(wide.begin(), wide.end(), 0x5555);
    Require(u_strFromUTF8Lenient_nid_postfix(wide.data(), total, &length, bytes.c_str(), -1, &error) == wide.data() && error == -124 && length == total &&
        std::memcmp(wide.data(), units.data(), units.size() * 2) == 0 && wide[units.size()] == 0x5555, "lenient chunked unterminated output");
    error = 0;
    const std::int32_t capacity = 0x900;
    std::fill(wide.begin(), wide.end(), 0x5555);
    Require(u_strFromUTF8Lenient_nid_postfix(wide.data(), capacity, &length, bytes.c_str(), -1, &error) == wide.data() && error == 15 && length == total,
        "lenient chunked overflow status");
    Require(std::memcmp(wide.data(), units.data(), capacity * 2) == 0 && wide[capacity] == 0x5555, "lenient chunked overflow prefix");
}

} // namespace

int main() {
    Scalars();
    Capacity();
    Malformed();
    Status();
    SubstitutionGeometry();
    Empty();
    LongStrings();
    LenientGuestPointers();
    LenientPageBoundaries();
    LenientChunks();
}
