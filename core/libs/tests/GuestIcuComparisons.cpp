#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
std::int32_t APS5_VABI u_strlen_nid_postfix(const char16_t*);
std::int32_t APS5_VABI u_strcmp_nid_postfix(const char16_t*, const char16_t*);
std::int32_t APS5_VABI u_strncmp_nid_postfix(const char16_t*, const char16_t*, std::int32_t);
std::int32_t APS5_VABI u_memcmp_nid_postfix(const char16_t*, const char16_t*, std::int32_t);
std::int32_t APS5_VABI u_memcmpCodePointOrder_nid_postfix(const char16_t*, const char16_t*, std::int32_t);
}

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
}

} // namespace

int main() {
    Exhaustive();
    Boundaries();
}
