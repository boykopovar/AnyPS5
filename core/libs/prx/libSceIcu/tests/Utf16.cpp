#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
#include "prx/libSceIcu/libSceIcu.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif



namespace {

void Require(bool value) {
    if (!value) throw std::runtime_error("ICU UTF-16 scalar test failed");
}

bool Pair(std::uint16_t first, std::uint16_t second) {
    return (first & 0xfc00) == 0xd800 && (second & 0xfc00) == 0xdc00;
}

std::size_t TerminatedLength(const std::vector<std::uint16_t>& text) {
    return std::find(text.begin(), text.end(), 0) - text.begin();
}

const std::uint16_t* ExpectedFind(const std::vector<std::uint16_t>& text, std::int32_t scalar, std::size_t length, bool last) {
    const std::uint16_t* result = nullptr;
    if (static_cast<std::uint32_t>(scalar) > 0x10ffff) return result;
    for (std::size_t i = 0; i < length; ++i) {
        bool match;
        if (scalar < 0x10000) {
            match = text[i] == scalar;
            if (i != 0 && Pair(text[i - 1], text[i])) match = false;
            if (i + 1 < length && Pair(text[i], text[i + 1])) match = false;
        } else {
            const auto value = static_cast<std::uint32_t>(scalar) - 0x10000;
            match = i + 1 < length && text[i] == 0xd800 + (value >> 10) && text[i + 1] == 0xdc00 + (value & 0x3ff);
        }
        if (match) {
            result = text.data() + i;
            if (!last) break;
        }
    }
    return result;
}

void Check(std::vector<std::uint16_t> text) {
    const auto payload = text.size();
    text.push_back(0);
    const auto original = text;
    constexpr std::array<std::int32_t, 16> scalars{0, 0x61, 0x62, 0xffff, 0xd800, 0xdbff, 0xdc00, 0xdfff, 0x10000, 0x103ff, 0x10fc00, 0x10ffff, -1, 0x110000, std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::min()};
    for (std::int32_t length = -1; length <= static_cast<std::int32_t>(payload); ++length) {
        const auto units = length < 0 ? TerminatedLength(text) : static_cast<std::size_t>(length);
        auto expectedCount = static_cast<std::int32_t>(units);
        for (std::size_t i = 1; i < units; ++i) if (Pair(text[i - 1], text[i])) --expectedCount;
        Require(u_countChar32_nid_postfix(text.data(), length) == expectedCount);
        for (std::int32_t threshold = -1; threshold <= static_cast<std::int32_t>(payload) + 1; ++threshold)
            Require(u_strHasMoreChar32Than_nid_postfix(text.data(), length, threshold) == (expectedCount > threshold));
        for (const auto scalar : scalars) {
            if (length >= 0) Require(u_memchr32_nid_postfix(text.data(), scalar, length) == ExpectedFind(text, scalar, units, false));
            else {
                Require(u_strchr32_nid_postfix(text.data(), scalar) == ExpectedFind(text, scalar, units + 1, false));
                Require(u_strrchr32_nid_postfix(text.data(), scalar) == ExpectedFind(text, scalar, units + 1, true));
            }
        }
    }
    Require(text == original);
}

void Enumerate(std::vector<std::uint16_t>& text, unsigned remaining) {
    Check(text);
    if (remaining == 0) return;
    for (const auto unit : std::array<std::uint16_t, 6>{0, 0x61, 0xd800, 0xdbff, 0xdc00, 0xdfff}) {
        text.push_back(unit);
        Enumerate(text, remaining - 1);
        text.pop_back();
    }
}

void Domains() {
    for (const auto length : {-2, -1, 0, 1, std::numeric_limits<std::int32_t>::max()}) {
        Require(u_countChar32_nid_postfix(nullptr, length) == 0);
        Require(u_strHasMoreChar32Than_nid_postfix(nullptr, length, 0) == 0);
        Require(u_strHasMoreChar32Than_nid_postfix(nullptr, length, -1) == 1);
    }
    const std::uint16_t empty[] = {0};
    Require(u_countChar32_nid_postfix(empty, -2) == 0);
    Require(u_strHasMoreChar32Than_nid_postfix(empty, -2, 0) == 0);
    Require(u_strHasMoreChar32Than_nid_postfix(empty, 0, -1) == 1);
    Require(u_strHasMoreChar32Than_nid_postfix(empty, std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max()) == 0);
    Require(u_strHasMoreChar32Than_nid_postfix(empty, std::numeric_limits<std::int32_t>::max(), 0) == 1);
    for (const auto scalar : {-1, 0x110000, std::numeric_limits<std::int32_t>::max()}) {
        Require(u_strchr32_nid_postfix(nullptr, scalar) == nullptr);
        Require(u_strrchr32_nid_postfix(nullptr, scalar) == nullptr);
        Require(u_memchr32_nid_postfix(nullptr, scalar, 5) == nullptr);
    }
    for (const auto count : {-2, -1, 0}) Require(u_memchr32_nid_postfix(nullptr, 0x61, count) == nullptr);
    for (const auto scalar : {0xd800, 0xdbff, 0xdc00, 0xdfff}) {
        Require(u_strchr32_nid_postfix(nullptr, scalar) == nullptr);
        Require(u_strrchr32_nid_postfix(nullptr, scalar) == nullptr);
        for (const auto count : {1, 2, std::numeric_limits<std::int32_t>::max()}) Require(u_memchr32_nid_postfix(nullptr, scalar, count) == nullptr);
    }
    for (const auto scalar : {0x10000, 0x10ffff}) Require(u_memchr32_nid_postfix(nullptr, scalar, 1) == nullptr);
    for (unsigned operation = 0; operation < 3; ++operation) {
        bool rejected = false;
        try {
            if (operation == 0) u_strchr32_nid_postfix(nullptr, 0x61);
            if (operation == 1) u_strrchr32_nid_postfix(nullptr, 0x61);
            if (operation == 2) u_memchr32_nid_postfix(nullptr, 0x61, 1);
        } catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected);
    }
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, 2 * size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(memory + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        memory = static_cast<char*>(mmap(nullptr, 2 * size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + size, size, PROT_NONE) == 0);
#endif
    }
    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 2 * size);
#endif
    }
    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;
    std::uint16_t* Tail(std::size_t units) const { return reinterpret_cast<std::uint16_t*>(memory + size) - units; }
    void ReadOnly() const {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(memory, size, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(memory, size, PROT_READ) == 0);
#endif
    }

private:
    char* memory = nullptr;
    std::size_t size = 0;
};

template <typename TCall>
void RequireUnreadable(TCall call) {
    errno = 71;
    bool rejected = false;
    try {
        call();
    } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected && errno == 71);
}

void GuestPointers() {
    const auto* unmapped = reinterpret_cast<const std::uint16_t*>(std::uintptr_t{0x10});
    for (const auto length : {-1, 1, 4}) RequireUnreadable([&] { u_countChar32_nid_postfix(unmapped, length); });
    RequireUnreadable([&] { u_strHasMoreChar32Than_nid_postfix(unmapped, -1, 1); });
    RequireUnreadable([&] { u_strHasMoreChar32Than_nid_postfix(unmapped, 4, 2); });
    GuardedPage page;
    const std::uint16_t text[] = {0x61, 0xd800, 0xdc00, 0x62, 0};
    auto* tail = page.Tail(5);
    std::memcpy(tail, text, sizeof(text));
    auto* open = page.Tail(2);
    page.ReadOnly();
    Require(tail[4] == 0);
    errno = 71;
    Require(u_countChar32_nid_postfix(tail, -1) == 3 && errno == 71);
    Require(u_countChar32_nid_postfix(tail, 5) == 4);
    Require(u_strHasMoreChar32Than_nid_postfix(tail, -1, 2) == 1);
    Require(u_strHasMoreChar32Than_nid_postfix(tail, -1, 3) == 0);
    Require(u_strHasMoreChar32Than_nid_postfix(tail + 1, 3, 2) == 0 && errno == 71);
    RequireUnreadable([&] { u_countChar32_nid_postfix(open, 3); });
    RequireUnreadable([&] { u_strHasMoreChar32Than_nid_postfix(open, 4, 2); });
    RequireUnreadable([&] { u_strchr32_nid_postfix(unmapped, 0x61); });
    RequireUnreadable([&] { u_strrchr32_nid_postfix(unmapped, 0x61); });
    RequireUnreadable([&] { u_memchr32_nid_postfix(unmapped, 0x61, 4); });
    errno = 71;
    Require(u_strchr32_nid_postfix(tail, 0x10000) == tail + 1);
    Require(u_strrchr32_nid_postfix(tail, 0) == tail + 4);
    Require(u_memchr32_nid_postfix(tail, 0x62, 5) == tail + 3 && errno == 71);
    Require(u_memchr32_nid_postfix(open, 0x62, 3) == open);
    RequireUnreadable([&] { u_memchr32_nid_postfix(open, 0x63, 3); });

    GuardedPage unterminated;
    auto* run = unterminated.Tail(3);
    run[0] = 0x61;
    run[1] = 0xd800;
    run[2] = 0xdc00;
    unterminated.ReadOnly();
    Require(u_strchr32_nid_postfix(run, 0x61) == run);
    Require(u_strchr32_nid_postfix(run, 0x10000) == run + 1);
    RequireUnreadable([&] { u_strchr32_nid_postfix(run, 0x62); });
    RequireUnreadable([&] { u_strrchr32_nid_postfix(run, 0x61); });
}

} // namespace

int main() {
    std::vector<std::uint16_t> text;
    Enumerate(text, 5);
    Check({0xffff, 0xdbff, 0xdfff, 0xd800, 0xdc00, 0x61, 0xd800, 0xdc00});
    Domains();
    GuestPointers();
}
