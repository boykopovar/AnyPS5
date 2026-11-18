#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif



namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU comparisons: %s\n", message);
    std::abort();
}

int Sign(std::int32_t value) {
    return (value > 0) - (value < 0);
}

struct Points {
    std::array<std::uint32_t, 3> values{};
    std::size_t size = 0;
};

Points Decode(const char16_t* units, std::size_t count) {
    Points points;
    for (std::size_t index = 0; index < count; ++index) {
        const auto current = static_cast<std::uint32_t>(units[index]);
        if (current >= 0xd800 && current <= 0xdbff && index + 1 < count &&
            units[index + 1] >= 0xdc00 && units[index + 1] <= 0xdfff) {
            points.values[points.size++] = 0x10000 + (current - 0xd800) * 0x400 + units[++index] - 0xdc00;
        } else points.values[points.size++] = current;
    }
    return points;
}

int ComparePoints(const char16_t* first, std::size_t firstCount, const char16_t* second, std::size_t secondCount) {
    const auto left = Decode(first, firstCount);
    const auto right = Decode(second, secondCount);
    const auto mismatch = std::mismatch(left.values.begin(), left.values.begin() + left.size,
                                        right.values.begin(), right.values.begin() + right.size);
    if (mismatch.first == left.values.begin() + left.size) return mismatch.second == right.values.begin() + right.size ? 0 : -1;
    if (mismatch.second == right.values.begin() + right.size) return 1;
    return *mismatch.first < *mismatch.second ? -1 : 1;
}

std::int32_t CompareUnits(const char16_t* first, const char16_t* second, std::size_t count) {
    const auto different = std::mismatch(first, first + count, second);
    return different.first == first + count ? 0 : static_cast<std::int32_t>(*different.first) - *different.second;
}

void Exhaustive() {
    constexpr std::array<char16_t, 9> alphabet{0, u'a', 0xd7ff, 0xd800, 0xdbff, 0xdc00, 0xdfff, 0xe000, 0xffff};
    std::vector<std::array<char16_t, 4>> strings;
    for (auto first : alphabet) for (auto second : alphabet) for (auto third : alphabet)
        strings.push_back({first, second, third, 0});
    const auto saved = strings;
    for (const auto& first : strings) {
        const auto length = std::char_traits<char16_t>::length(first.data());
        Require(u_strlen_nid_postfix(first.data()) == static_cast<std::int32_t>(length), "length counts code units through first NUL");
        for (const auto& second : strings) {
            const auto secondLength = std::char_traits<char16_t>::length(second.data());
            const auto terminatedCount = std::min(length, secondLength) + 1;
            const auto terminatedDifference = CompareUnits(first.data(), second.data(), terminatedCount);
            Require(u_strcmp_nid_postfix(first.data(), second.data()) == terminatedDifference, "exact terminated code-unit difference");
            for (std::int32_t count = 0; count <= 3; ++count) {
                const auto expected = CompareUnits(first.data(), second.data(), count);
                Require(u_memcmp_nid_postfix(first.data(), second.data(), count) == expected, "exact binary difference including NUL");
                Require(u_strncmp_nid_postfix(first.data(), second.data(), count) ==
                        CompareUnits(first.data(), second.data(), std::min<std::size_t>(count, terminatedCount)), "bounded string stops at NUL or bound");
                Require(Sign(u_memcmpCodePointOrder_nid_postfix(first.data(), second.data(), count)) ==
                        ComparePoints(first.data(), count, second.data(), count), "scalar-value ordering with paired and unpaired surrogates");
            }
            Require(Sign(u_memcmpCodePointOrder_nid_postfix(first.data(), second.data(), -1)) ==
                    ComparePoints(first.data(), length, second.data(), secondLength), "negative code-point count uses string lengths");
        }
    }
    Require(strings == saved, "all inputs remain unchanged");
}

void Boundaries() {
    const std::array<char16_t, 3> supplementary{0xd800, 0xdc00, 0};
    const std::array<char16_t, 3> bmp{0xe000, 0, 0};
    Require(u_strlen_nid_postfix(supplementary.data()) == 2, "supplementary character occupies two code units");
    Require(u_memcmpCodePointOrder_nid_postfix(supplementary.data(), bmp.data(), 1) == -0x800, "boundary excludes trailing surrogate");
    Require(u_memcmpCodePointOrder_nid_postfix(supplementary.data(), bmp.data(), 2) == 0x2000, "paired lead sorts after upper BMP with exact difference");
    const std::array<char16_t, 3> longer{u'a', u'b', 0};
    const std::array<char16_t, 3> shorter{u'a', 0, 0};
    Require(u_memcmpCodePointOrder_nid_postfix(shorter.data(), longer.data(), -1) == -98, "negative length compares the first terminator exactly");
    Require(u_memcmpCodePointOrder_nid_postfix(longer.data(), shorter.data(), std::numeric_limits<std::int32_t>::min()) == 98, "all negative counts select terminated lengths");
    Require(u_strncmp_nid_postfix(u"a", u"b", std::numeric_limits<std::int32_t>::max()) == -1, "large bound stops at mismatch");
    Require(u_strncmp_nid_postfix(u"", u"", std::numeric_limits<std::int32_t>::max()) == 0, "large bound stops at terminator");
    for (const auto count : {0, -1, std::numeric_limits<std::int32_t>::min()}) {
        Require(u_memcmp_nid_postfix(nullptr, nullptr, count) == 0, "nonpositive binary count reads nothing");
        Require(u_strncmp_nid_postfix(nullptr, nullptr, count) == 0, "nonpositive string bound reads nothing");
    }
    for (const auto count : {0, -1, 1, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()})
        Require(u_memcmpCodePointOrder_nid_postfix(nullptr, nullptr, count) == 0, "identical code-point buffers read nothing for every count");
    int rejected = 0;
    try { u_strlen_nid_postfix(nullptr); } catch (const std::invalid_argument&) { ++rejected; }
    try { u_strcmp_nid_postfix(nullptr, u""); } catch (const std::invalid_argument&) { ++rejected; }
    try { u_strncmp_nid_postfix(u"", nullptr, 1); } catch (const std::invalid_argument&) { ++rejected; }
    try { u_memcmp_nid_postfix(nullptr, u"", 1); } catch (const std::invalid_argument&) { ++rejected; }
    try { u_memcmpCodePointOrder_nid_postfix(u"", nullptr, -1); } catch (const std::invalid_argument&) { ++rejected; }
    Require(rejected == 5, "invalid required buffers throw");

    const auto* const unmapped = reinterpret_cast<const char16_t*>(std::uintptr_t{0x10});
    const auto* const otherUnmapped = reinterpret_cast<const char16_t*>(std::uintptr_t{0x20});
    errno = 71;
    Require(u_strncmp_nid_postfix(unmapped, u"", 0) == 0 && errno == 71, "zero strncmp count does not read unmapped pointer");
    Require(u_memcmp_nid_postfix(unmapped, u"", 0) == 0 && errno == 71, "zero memcmp count does not read unmapped pointer");
    Require(u_memcmpCodePointOrder_nid_postfix(unmapped, u"", 0) == 0 && errno == 71, "zero code-point count does not read unmapped pointer");
    Require(u_memcmpCodePointOrder_nid_postfix(unmapped, unmapped, 1) == 0 && errno == 71, "identical code-point unmapped pointers do not fault");
    int unreadableRejected = 0;
    try { errno = 71; u_strlen_nid_postfix(unmapped); } catch (const std::invalid_argument&) { Require(errno == 71, "u_strlen preserves errno"); ++unreadableRejected; }
    try { errno = 71; u_strcmp_nid_postfix(unmapped, u""); } catch (const std::invalid_argument&) { Require(errno == 71, "u_strcmp preserves errno on first"); ++unreadableRejected; }
    try { errno = 71; u_strcmp_nid_postfix(u"", otherUnmapped); } catch (const std::invalid_argument&) { Require(errno == 71, "u_strcmp preserves errno on second"); ++unreadableRejected; }
    try { errno = 71; u_strncmp_nid_postfix(unmapped, u"", 1); } catch (const std::invalid_argument&) { Require(errno == 71, "u_strncmp preserves errno"); ++unreadableRejected; }
    try { errno = 71; u_memcmp_nid_postfix(unmapped, u"", 1); } catch (const std::invalid_argument&) { Require(errno == 71, "u_memcmp preserves errno"); ++unreadableRejected; }
    try { errno = 71; u_memcmpCodePointOrder_nid_postfix(unmapped, u"", 1); } catch (const std::invalid_argument&) { Require(errno == 71, "u_memcmpCodePointOrder positive count preserves errno"); ++unreadableRejected; }
    try { errno = 71; u_memcmpCodePointOrder_nid_postfix(unmapped, u"", -1); } catch (const std::invalid_argument&) { Require(errno == 71, "u_memcmpCodePointOrder negative count preserves errno"); ++unreadableRejected; }
    Require(unreadableRejected == 7, "unreadable buffers throw invalid_argument");
}

class GuardedPages {
public:
    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size_ = info.dwPageSize;
        base_ = static_cast<unsigned char*>(VirtualAlloc(nullptr, size_ * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base_ != nullptr, "allocate guarded pages");
        DWORD previous = 0;
        Require(VirtualProtect(base_ + size_, size_, PAGE_NOACCESS, &previous) != 0, "protect trailing page");
#else
        size_ = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapping = mmap(nullptr, size_ * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED, "allocate guarded pages");
        base_ = static_cast<unsigned char*>(mapping);
        Require(mprotect(base_ + size_, size_, PROT_NONE) == 0, "protect trailing page");
#endif
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(base_, 0, MEM_RELEASE);
#else
        munmap(base_, size_ * 2);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    char16_t* Head() const { return reinterpret_cast<char16_t*>(base_); }
    char16_t* Tail(std::size_t units) const { return reinterpret_cast<char16_t*>(base_ + size_) - units; }

private:
    unsigned char* base_ = nullptr;
    std::size_t size_ = 0;
};

void Chunking() {
    std::u16string long_(5000, u'x');
    std::u16string longer = long_;
    longer.back() = u'y';
    Require(u_strlen_nid_postfix(long_.c_str()) == 5000, "strlen crosses several chunks");
    Require(u_strcmp_nid_postfix(long_.c_str(), std::u16string(long_).c_str()) == 0, "strcmp equal across chunks");
    Require(u_strcmp_nid_postfix(long_.c_str(), longer.c_str()) < 0, "strcmp finds the last differing unit");
    Require(u_strncmp_nid_postfix(long_.c_str(), longer.c_str(), 4999) == 0, "strncmp stops at the bound");
    Require(u_memcmp_nid_postfix(longer.c_str(), long_.c_str(), 5000) > 0, "memcmp finds the last differing unit");
    Require(u_memcmpCodePointOrder_nid_postfix(long_.c_str(), longer.c_str(), 5000) < 0, "code-point order across chunks");
    Require(u_memcmpCodePointOrder_nid_postfix(long_.c_str(), longer.c_str(), -1) < 0, "code-point order with terminator scan");

    GuardedPages pages;
    constexpr std::size_t units = 32;
    char16_t* terminated = pages.Tail(units);
    std::fill_n(terminated, units - 1, u'a');
    terminated[units - 1] = 0;
    Require(u_strlen_nid_postfix(terminated) == static_cast<std::int32_t>(units - 1), "string ending at the last readable unit is read");
    Require(u_strcmp_nid_postfix(terminated, std::u16string(units - 1, u'a').c_str()) == 0, "strcmp stops before the guarded page");
    errno = 71;
    terminated[units - 1] = u'a';
    int rejected = 0;
    try { u_strlen_nid_postfix(terminated); } catch (const std::invalid_argument&) { Require(errno == 71, "strlen into a guarded page keeps errno"); ++rejected; }
    try { u_strcmp_nid_postfix(terminated, std::u16string(units, u'a').c_str()); } catch (const std::invalid_argument&) { Require(errno == 71, "strcmp into a guarded page keeps errno"); ++rejected; }
    Require(rejected == 2, "unterminated strings fault into the guarded page");

    constexpr std::size_t lateUnits = 100;
    char16_t* late = pages.Tail(lateUnits);
    std::fill_n(late, lateUnits - 1, u'a');
    late[lateUnits - 1] = 0;
    const std::u16string lateCopy(lateUnits - 1, u'a');
    Require(u_strlen_nid_postfix(late) == static_cast<std::int32_t>(lateUnits - 1), "string growing past a chunk ends at the last readable unit");
    Require(u_strcmp_nid_postfix(late, lateCopy.c_str()) == 0, "strcmp across grown chunks stops before the guarded page");
    Require(u_memcmpCodePointOrder_nid_postfix(late, lateCopy.c_str(), -1) == 0, "code-point order across grown chunks stops before the guarded page");
}

void ChunkBoundaries() {
    GuardedPages leftPage;
    GuardedPages rightPage;
    char16_t* const left = leftPage.Head();
    char16_t* const right = rightPage.Head();
    constexpr std::size_t length = 120;
    const auto reset = [&] {
        std::fill_n(left, length, u'a');
        std::fill_n(right, length, u'a');
        left[length] = 0;
        right[length] = 0;
    };
    for (const std::size_t boundary : {std::size_t{16}, std::size_t{48}, std::size_t{112}}) {
        const auto count = static_cast<std::int32_t>(boundary);
        reset();
        Require(u_strlen_nid_postfix(left) == static_cast<std::int32_t>(length), "strlen of equal strings crossing a chunk boundary");
        Require(u_strcmp_nid_postfix(left, right) == 0, "strcmp of equal strings crossing a chunk boundary");
        Require(u_strncmp_nid_postfix(left, right, count + 1) == 0, "strncmp of equal strings crossing a chunk boundary");
        Require(u_memcmp_nid_postfix(left, right, count + 1) == 0, "memcmp of equal strings crossing a chunk boundary");
        Require(u_memcmpCodePointOrder_nid_postfix(left, right, count + 1) == 0, "code-point order of equal strings crossing a chunk boundary");
        Require(u_memcmpCodePointOrder_nid_postfix(left, right, -1) == 0, "terminated code-point order of equal strings crossing a chunk boundary");

        for (const std::size_t at : {boundary - 1, boundary}) {
            reset();
            right[at] = u'b';
            const auto through = static_cast<std::int32_t>(at + 1);
            const auto before = static_cast<std::int32_t>(at);
            Require(u_strcmp_nid_postfix(left, right) == u'a' - u'b', "strcmp finds a difference beside a chunk boundary");
            Require(u_strncmp_nid_postfix(left, right, before) == 0, "strncmp stops before a difference beside a chunk boundary");
            Require(u_strncmp_nid_postfix(left, right, through) == u'a' - u'b', "strncmp finds a difference beside a chunk boundary");
            Require(u_memcmp_nid_postfix(left, right, before) == 0, "memcmp stops before a difference beside a chunk boundary");
            Require(u_memcmp_nid_postfix(left, right, through) == u'a' - u'b', "memcmp finds a difference beside a chunk boundary");
            Require(u_memcmpCodePointOrder_nid_postfix(left, right, through) < 0, "code-point order finds a difference beside a chunk boundary");
            Require(u_memcmpCodePointOrder_nid_postfix(left, right, -1) < 0, "terminated code-point order finds a difference beside a chunk boundary");
        }

        reset();
        left[boundary] = 0;
        Require(u_strlen_nid_postfix(left) == count, "strlen stops at a NUL on a chunk boundary");
        Require(u_strcmp_nid_postfix(left, right) == -static_cast<std::int32_t>(u'a'), "strcmp stops at a NUL on a chunk boundary");
        Require(u_strncmp_nid_postfix(left, right, count) == 0, "strncmp up to a NUL on a chunk boundary");
        Require(u_strncmp_nid_postfix(left, right, count + 1) == -static_cast<std::int32_t>(u'a'), "strncmp through a NUL on a chunk boundary");
        Require(u_memcmp_nid_postfix(left, right, count + 1) == -static_cast<std::int32_t>(u'a'), "memcmp reads past a NUL on a chunk boundary");
        Require(u_memcmpCodePointOrder_nid_postfix(left, right, -1) < 0, "terminated code-point order stops at a NUL on a chunk boundary");
    }
}

} // namespace

int main() {
    Exhaustive();
    Boundaries();
    Chunking();
    ChunkBoundaries();
}
