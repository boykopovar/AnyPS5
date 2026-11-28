#include "prx/libSceIcu/libSceIcu.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu string hash: " + message);
}

template <typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template <typename Unit>
std::int32_t Reference(const Unit* s, std::int32_t length, bool fold) {
    std::uint32_t hash = 0;
    if (!s) return 0;
    const std::int64_t step = (length - 32) / 32 + 1;
    for (std::int64_t offset = 0; offset < length; offset += step) {
        std::uint32_t unit = static_cast<std::uint32_t>(s[offset]);
        if (fold && unit >= 'A' && unit <= 'Z') unit += 'a' - 'A';
        hash = hash * 37 + unit;
    }
    return static_cast<std::int32_t>(hash);
}

struct GuardedPages {
    char* begin = nullptr;
    std::size_t size = 0;
    std::size_t readable = 0;

    explicit GuardedPages(std::size_t bytes) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
        readable = (bytes + size - 1) / size * size;
#ifdef _WIN32
        begin = static_cast<char*>(VirtualAlloc(nullptr, readable + size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(begin != nullptr, "VirtualAlloc failed");
        DWORD previous = 0;
        Require(VirtualProtect(begin + readable, size, PAGE_NOACCESS, &previous) != 0, "VirtualProtect failed");
#else
        void* mapped = mmap(nullptr, readable + size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "mmap failed");
        begin = static_cast<char*>(mapped);
        Require(mprotect(begin + readable, size, PROT_NONE) == 0, "mprotect failed");
#endif
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(begin, 0, MEM_RELEASE);
#else
        munmap(begin, readable + size);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    char* Guard() const { return begin + readable; }
};

std::string Text(std::size_t length) {
    std::string text(length, 0);
    std::uint32_t state = 12345;
    for (char& c : text) {
        state = state * 1103515245 + 12345;
        c = static_cast<char>(' ' + (state >> 16) % 95);
    }
    return text;
}

std::vector<std::uint16_t> Units(std::size_t length) {
    std::vector<std::uint16_t> units(length);
    std::uint32_t state = 54321;
    for (auto& unit : units) {
        state = state * 1103515245 + 12345;
        unit = static_cast<std::uint16_t>(state >> 15);
    }
    return units;
}

void KnownValues() {
    const std::uint16_t wide[] = {0x61, 0x62, 0x63};
    Require(ustr_hashUCharsN_nid_postfix(wide, 3) == 136518, "ustr_hashUCharsN hashed abc wrongly");
    Require(ustr_hashCharsN_nid_postfix("abc", 3) == 136518, "ustr_hashCharsN hashed abc wrongly");
    Require(ustr_hashICharsN_nid_postfix("ABC", 3) == 136518, "ustr_hashICharsN did not fold case");
    Require(ustr_hashICharsN_nid_postfix("[@", 2) == ustr_hashCharsN_nid_postfix("[@", 2),
        "ustr_hashICharsN folded a character outside A-Z");
    Require(ustr_hashCharsN_nid_postfix("\xff", 1) == 255, "ustr_hashCharsN did not hash a high byte as unsigned");
}

void MatchesReference() {
    const std::string text = Text(300000);
    const std::vector<std::uint16_t> units = Units(300000);
    for (std::int32_t length : {1, 2, 31, 32, 33, 63, 64, 65, 95, 96, 97, 127, 128, 1000, 4095, 4096, 4097,
                                65535, 131103, 131104, 299999, 300000}) {
        Require(ustr_hashCharsN_nid_postfix(text.data(), length) == Reference(
            reinterpret_cast<const std::uint8_t*>(text.data()), length, false),
            "ustr_hashCharsN differs from ICU at length " + std::to_string(length));
        Require(ustr_hashICharsN_nid_postfix(text.data(), length) == Reference(
            reinterpret_cast<const std::uint8_t*>(text.data()), length, true),
            "ustr_hashICharsN differs from ICU at length " + std::to_string(length));
        Require(ustr_hashUCharsN_nid_postfix(units.data(), length) == Reference(units.data(), length, false),
            "ustr_hashUCharsN differs from ICU at length " + std::to_string(length));
    }
}

void NullAndUnreadable() {
    const auto* bytes = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    const auto* units = reinterpret_cast<const std::uint16_t*>(std::uintptr_t{0x10});
    Require(ustr_hashCharsN_nid_postfix(nullptr, 5) == 0, "ustr_hashCharsN did not hash a null string to zero");
    Require(ustr_hashICharsN_nid_postfix(nullptr, 5) == 0, "ustr_hashICharsN did not hash a null string to zero");
    Require(ustr_hashUCharsN_nid_postfix(nullptr, 5) == 0, "ustr_hashUCharsN did not hash a null string to zero");
    Require(ustr_hashCharsN_nid_postfix(bytes, 0) == 0, "ustr_hashCharsN read an empty string");
    Require(ustr_hashICharsN_nid_postfix(bytes, -1) == 0, "ustr_hashICharsN read a string of negative length");
    Require(ustr_hashUCharsN_nid_postfix(units, 0) == 0, "ustr_hashUCharsN read an empty string");
    errno = 71;
    Require(Throws([&] { ustr_hashCharsN_nid_postfix(bytes, 3); }), "ustr_hashCharsN read an unreadable string");
    Require(Throws([&] { ustr_hashICharsN_nid_postfix(bytes, 3); }), "ustr_hashICharsN read an unreadable string");
    Require(Throws([&] { ustr_hashUCharsN_nid_postfix(units, 3); }), "ustr_hashUCharsN read an unreadable string");
    Require(errno == 71, "an unreadable string changed errno");
}

void OnlySampledUnitsAreRead() {
    constexpr std::int32_t length = 300000;
    constexpr std::int32_t step = (length - 32) / 32 + 1;
    constexpr std::int32_t last = (length - 1) / step * step;
    const std::string text = Text(length);
    {
        GuardedPages pages(last + 1);
        char* s = pages.Guard() - (last + 1);
        std::memcpy(s, text.data(), last + 1);
        const auto* reference = reinterpret_cast<const std::uint8_t*>(text.data());
        Require(ustr_hashCharsN_nid_postfix(s, length) == Reference(reference, length, false),
            "ustr_hashCharsN read a unit ICU skips");
        Require(ustr_hashICharsN_nid_postfix(s, length) == Reference(reference, length, true),
            "ustr_hashICharsN read a unit ICU skips");
        Require(Throws([&] { ustr_hashCharsN_nid_postfix(s + 1, length); }),
            "ustr_hashCharsN did not read the sample past the guard");
    }
    const std::vector<std::uint16_t> units = Units(length);
    {
        GuardedPages pages(sizeof(std::uint16_t) * (last + 1));
        auto* s = reinterpret_cast<std::uint16_t*>(pages.Guard()) - (last + 1);
        std::memcpy(s, units.data(), sizeof(std::uint16_t) * (last + 1));
        Require(ustr_hashUCharsN_nid_postfix(s, length) == Reference(units.data(), length, false),
            "ustr_hashUCharsN read a unit ICU skips");
        Require(Throws([&] { ustr_hashUCharsN_nid_postfix(s + 1, length); }),
            "ustr_hashUCharsN did not read the sample past the guard");
    }
}

}

int main() {
    KnownValues();
    MatchesReference();
    NullAndUnreadable();
    OnlySampledUnitsAreRead();
    return 0;
}
