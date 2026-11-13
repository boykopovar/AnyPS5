#include "prx/libSceLibcInternal/include/Ctype.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Facet = GuestLocale::CtypeFacet;
using Scalar = char (APS5_VABI *)(const Facet*, char);
using Range = const char* (APS5_VABI *)(const Facet*, char*, const char*);

struct Methods {
    Scalar scalar;
    Range range;
    bool upper;
};

constexpr std::array<Methods, 2> CaseMethods{{
    {_ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix, _ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix, false},
    {_ZNKSt5ctypeIcE10do_toupperEc_nid_postfix, _ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix, true}
}};

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "Ctype case check failed at line %u\n", location.line());
    std::abort();
}

template<class Function>
void RequireInvalid(Function function) {
    bool thrown = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
}

unsigned char Mapped(unsigned value, bool upper) {
    return static_cast<unsigned char>(upper ? value * 151 + 201 : value * 73 + 19);
}

struct Tables {
    std::array<std::int16_t, 256> lower;
    std::array<std::int16_t, 256> upper;

    Tables() {
        for (unsigned value = 0; value < 256; ++value) {
            lower[value] = static_cast<std::int16_t>(Mapped(value, false) + (value % 2 ? -0x7100 : 0x3d00));
            upper[value] = static_cast<std::int16_t>(Mapped(value, true) + (value % 2 ? 0x6200 : -0x4500));
        }
    }

    Facet MakeFacet() const {
        Facet facet{};
        facet.lower = lower.data();
        facet.upper = upper.data();
        return facet;
    }
};

void CheckEveryByte() {
    const Tables tables;
    Facet facet = tables.MakeFacet();
    Facet swapped = facet;
    swapped.lower = tables.upper.data();
    swapped.upper = tables.lower.data();
    for (const auto& method : CaseMethods) {
        for (unsigned value = 0; value < 256; ++value) {
            const char input = static_cast<char>(value);
            Require(static_cast<unsigned char>(method.scalar(&facet, input)) == Mapped(value, method.upper));
            Require(static_cast<unsigned char>(method.scalar(&swapped, input)) == Mapped(value, !method.upper));
        }
        std::array<char, 258> bytes{};
        bytes.front() = static_cast<char>(0xa5);
        bytes.back() = static_cast<char>(0x5a);
        for (unsigned value = 0; value < 256; ++value) bytes[value + 1] = static_cast<char>(value);
        Require(method.range(&facet, bytes.data() + 1, bytes.data() + 257) == bytes.data() + 257);
        for (unsigned value = 0; value < 256; ++value) Require(static_cast<unsigned char>(bytes[value + 1]) == Mapped(value, method.upper));
        Require(static_cast<unsigned char>(bytes.front()) == 0xa5 && static_cast<unsigned char>(bytes.back()) == 0x5a);
        std::array<char, 7> subrange{'q', 'A', '\0', static_cast<char>(0x80), static_cast<char>(0xff), 'z', ')'};
        const auto original = subrange;
        Require(method.range(&facet, subrange.data() + 1, subrange.data() + 6) == subrange.data() + 6);
        for (unsigned index = 1; index < 6; ++index) Require(static_cast<unsigned char>(subrange[index]) == Mapped(static_cast<unsigned char>(original[index]), method.upper));
        Require(subrange.front() == original.front() && subrange.back() == original.back());
    }
}

void CheckClassicTables() {
    Facet facet{};
    facet.lower = _Getptolower_nid_postfix();
    facet.upper = _Getptoupper_nid_postfix();
    for (const auto& method : CaseMethods) {
        std::array<char, 256> bytes{};
        for (unsigned value = 0; value < 256; ++value) bytes[value] = static_cast<char>(value);
        Require(method.range(&facet, bytes.data(), bytes.data() + bytes.size()) == bytes.data() + bytes.size());
        for (unsigned value = 0; value < 256; ++value) {
            unsigned expected = value;
            if (method.upper && value >= 'a' && value <= 'z') expected -= 32;
            if (!method.upper && value >= 'A' && value <= 'Z') expected += 32;
            Require(static_cast<unsigned char>(bytes[value]) == expected);
            Require(static_cast<unsigned char>(method.scalar(&facet, static_cast<char>(value))) == expected);
        }
    }
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous;
        Require(VirtualProtect(memory + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        memory = static_cast<char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + size, size, PROT_NONE) == 0);
#endif
        Require(size >= 4096);
    }

    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, size * 2);
#endif
    }

    char* Data() { return memory; }
    char* End() { return memory + size; }

    void SetWritable(bool writable) {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory, size, writable ? PAGE_READWRITE : PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(memory, size, writable ? PROT_READ | PROT_WRITE : PROT_READ) == 0);
#endif
    }

private:
    char* memory;
    std::size_t size;
};

void CheckEmptyAndInvalidRanges() {
    const Tables tables;
    const Facet facet = tables.MakeFacet();
    GuardedPage page;
    for (const auto& method : CaseMethods) {
        Require(method.range(nullptr, nullptr, nullptr) == nullptr);
        Require(method.range(nullptr, page.End(), page.End()) == page.End());
        char byte = 'x';
        Require(method.range(nullptr, &byte, &byte) == &byte && byte == 'x');
        std::array<char, 2> bytes{'A', 'B'};
        RequireInvalid([&] { method.range(&facet, bytes.data() + 1, bytes.data()); });
        Require(bytes[0] == 'A' && bytes[1] == 'B');
        RequireInvalid([&] { method.range(nullptr, bytes.data(), bytes.data() + bytes.size()); });
        Require(bytes[0] == 'A' && bytes[1] == 'B');
        RequireInvalid([&] { method.range(&facet, page.End(), page.End() + 1); });
    }
}

void CheckPointerFaults() {
    const Tables tables;
    GuardedPage page;
    for (const auto& method : CaseMethods) {
        Facet facet = tables.MakeFacet();
        RequireInvalid([&] { method.scalar(nullptr, 'A'); });
        RequireInvalid([&] { method.scalar(reinterpret_cast<const Facet*>(page.End()), 'A'); });
        (method.upper ? facet.upper : facet.lower) = nullptr;
        RequireInvalid([&] { method.scalar(&facet, 'A'); });
        (method.upper ? facet.upper : facet.lower) = reinterpret_cast<const std::int16_t*>(page.End());
        RequireInvalid([&] { method.scalar(&facet, 'A'); });
        facet = tables.MakeFacet();
        page.Data()[0] = 'A';
        page.SetWritable(false);
        RequireInvalid([&] { method.range(&facet, page.Data(), page.Data() + 1); });
        Require(page.Data()[0] == 'A');
        page.SetWritable(true);
        page.End()[-1] = 'A';
        RequireInvalid([&] { method.range(&facet, page.End() - 1, page.End() + 1); });
        Require(static_cast<unsigned char>(page.End()[-1]) == Mapped('A', method.upper));
        page.End()[-2] = 'B';
        (method.upper ? facet.upper : facet.lower) = reinterpret_cast<const std::int16_t*>(page.End() - 2);
        std::array<char, 2> bytes{0, 1};
        RequireInvalid([&] { method.range(&facet, bytes.data(), bytes.data() + bytes.size()); });
        Require(bytes[0] == 'B' && bytes[1] == 1);
    }
}

void CheckMappedReads() {
    const Tables tables;
    GuardedPage page;
    for (const auto& method : CaseMethods) {
        Facet facet = tables.MakeFacet();
        std::memcpy(page.Data(), &facet, sizeof(facet));
        page.SetWritable(false);
        const auto* mappedFacet = reinterpret_cast<const Facet*>(page.Data());
        Require(static_cast<unsigned char>(method.scalar(mappedFacet, 'A')) == Mapped('A', method.upper));
        char byte = 'A';
        Require(method.range(mappedFacet, &byte, &byte + 1) == &byte + 1);
        Require(static_cast<unsigned char>(byte) == Mapped('A', method.upper));
        page.SetWritable(true);
        const std::int16_t* table = method.upper ? tables.upper.data() : tables.lower.data();
        std::memcpy(page.End() - sizeof(table), &table, sizeof(table));
        const std::size_t offset = method.upper ? offsetof(Facet, upper) : offsetof(Facet, lower);
        mappedFacet = reinterpret_cast<const Facet*>(page.End() - sizeof(table) - offset);
        Require(static_cast<unsigned char>(method.scalar(mappedFacet, 'A')) == Mapped('A', method.upper));
        page.End()[-1] = static_cast<char>(0xd3);
        table = reinterpret_cast<const std::int16_t*>(page.End() - 1 - 2 * 0x80);
        (method.upper ? facet.upper : facet.lower) = table;
        page.SetWritable(false);
        Require(static_cast<unsigned char>(method.scalar(&facet, static_cast<char>(0x80))) == 0xd3);
        byte = static_cast<char>(0x80);
        Require(method.range(&facet, &byte, &byte + 1) == &byte + 1);
        Require(static_cast<unsigned char>(byte) == 0xd3);
        page.SetWritable(true);
    }
}

void CheckTablePointerReload() {
    GuardedPage page;
    auto* firstTable = reinterpret_cast<std::int16_t*>(page.Data() + 0x200);
    auto* secondTable = reinterpret_cast<std::int16_t*>(page.Data() + 0x210);
    for (const auto& method : CaseMethods) {
        Facet facet{};
        auto& table = method.upper ? facet.upper : facet.lower;
        table = firstTable;
        auto* bytes = reinterpret_cast<char*>(&table);
        const unsigned index = static_cast<unsigned char>(bytes[1]);
        Require(static_cast<unsigned char>(bytes[0]) == 0 && index != 0 && index != 8);
        firstTable[0] = 0x10;
        firstTable[index] = 0x6b;
        secondTable[index] = 0x5a;
        Require(method.range(&facet, bytes, bytes + 2) == bytes + 2);
        Require(static_cast<unsigned char>(bytes[0]) == 0x10 && static_cast<unsigned char>(bytes[1]) == 0x5a);
        table = nullptr;
    }
}

}

int main() {
    CheckEveryByte();
    CheckClassicTables();
    CheckEmptyAndInvalidRanges();
    CheckPointerFaults();
    CheckMappedReads();
    CheckTablePointerReload();
}
