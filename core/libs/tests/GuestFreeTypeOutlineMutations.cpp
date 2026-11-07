#include "prx/libSceFreeType/include/Api.hpp"
#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType outline mutation check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static bool Equal(const GuestVector& left, const GuestVector& right) {
    return left.x == right.x && left.y == right.y;
}

static void ReverseOracle(std::size_t count, std::vector<std::uint16_t> contours, std::int32_t flags) {
    std::vector<GuestVector> storage(count + 2);
    storage.front() = {991, 992};
    storage.back() = {-993, -994};
    std::vector<unsigned char> tags(count + 2, 0xA5);
    for (std::size_t index = 0; index < count; ++index) {
        storage[index + 1] = {(std::int64_t{1} << 40) + static_cast<std::int64_t>(index) * 3,
                              -(std::int64_t{1} << 41) + static_cast<std::int64_t>(index) * 7};
        tags[index + 1] = static_cast<unsigned char>(index * 73);
    }
    const auto original = storage;
    const auto originalTags = tags;
    const auto originalContours = contours;
    GuestOutline guest{static_cast<std::uint16_t>(contours.size()), static_cast<std::uint16_t>(count), storage.data() + 1,
                       tags.data() + 1, contours.data(), flags};
    FT_Outline_Reverse_nid_postfix(&guest);

    Require(guest.flags == (flags ^ FT_OUTLINE_REVERSE_FILL));
    Require(guest.n_points == count && guest.n_contours == contours.size() && contours == originalContours);
    for (std::size_t index = 0; index < count; ++index) {
        const auto contour = std::lower_bound(contours.begin(), contours.end(), index);
        const std::size_t first = contour == contours.begin() ? 0 : *(contour - 1) + 1;
        const auto source = index == first ? first : first + *contour + 1 - index;
        Require(source < count && Equal(storage[index + 1], original[source + 1]));
        Require(tags[index + 1] == originalTags[source + 1]);
    }
    Require(Equal(storage.front(), original.front()) && Equal(storage.back(), original.back()));
    Require(tags.front() == 0xA5 && tags.back() == 0xA5);
    FT_Outline_Reverse_nid_postfix(&guest);

    Require(guest.flags == flags && tags == originalTags);
    for (std::size_t index = 0; index < storage.size(); ++index) Require(Equal(storage[index], original[index]));
}

static void TranslateOracle(std::size_t count, FT_Pos xOffset, FT_Pos yOffset) {
    std::vector<GuestVector> storage(count + 2, {991, -992});
    std::vector<FT_Vector> nativePoints;
    for (std::size_t index = 0; index < count; ++index) {
        const auto value = static_cast<FT_Pos>(index % 101);
        storage[index + 1] = {value - 50, 25 - value};
        nativePoints.push_back({value - 50, 25 - value});
    }
    GuestOutline guest{65535, static_cast<std::uint16_t>(count), storage.data() + 1, nullptr, nullptr, 0x71};
    FT_Outline native{guest.n_contours, guest.n_points, nativePoints.data(), nullptr, nullptr, guest.flags};
    FT_Outline_Translate(&native, xOffset, yOffset);
    FT_Outline_Translate_nid_postfix(&guest, xOffset, yOffset);

    for (std::size_t index = 0; index < count; ++index) {
        Require(storage[index + 1].x == nativePoints[index].x && storage[index + 1].y == nativePoints[index].y);
    }
    Require(Equal(storage.front(), {991, -992}) && Equal(storage.back(), {991, -992}));
    Require(guest.n_contours == 65535 && guest.n_points == count && guest.flags == 0x71);
    Require(guest.tags == nullptr && guest.contours == nullptr);
}

static void WideTranslation() {
    const auto low = std::numeric_limits<std::int64_t>::min();
    const auto high = std::numeric_limits<std::int64_t>::max();
    const auto wide = std::int64_t{1} << 40;
    std::array<GuestVector, 6> points = {{{high, low}, {low, high}, {-1, 0}, {1, -1}, {wide, -wide}, {high - 5, low + 5}}};
    const auto original = points;
    const std::array<GuestVector, 6> expected = {{{low, high}, {low + 1, high - 1}, {0, -1}, {2, -2}, {wide + 1, -wide - 1}, {high - 4, low + 4}}};
    GuestOutline guest{0, static_cast<std::uint16_t>(points.size()), points.data(), nullptr, nullptr, 0};
    FT_Outline_Translate_nid_postfix(&guest, 1, -1);

    for (std::size_t index = 0; index < points.size(); ++index) Require(Equal(points[index], expected[index]));
    FT_Outline_Translate_nid_postfix(&guest, -1, 1);

    for (std::size_t index = 0; index < points.size(); ++index) Require(Equal(points[index], original[index]));
    GuestVector point{7, -9};
    guest.n_points = 1;
    guest.points = &point;
    FT_Outline_Translate_nid_postfix(&guest, wide, -wide);

    Require(Equal(point, {wide + 7, -wide - 9}));
    point = {low, 1};
    FT_Outline_Translate_nid_postfix(&guest, low, high);

    Require(Equal(point, {0, low}));
}

static void RenderModifiedOutline() {
    const auto bytes = SquareGlyphFont('A', 700);
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    {
        GuestFace guest;
        guest.library = &library;
        FT_Face native = nullptr;
        Require(FT_New_Memory_Face(library.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &native) == 0);
        Require(FT_Set_Pixel_Sizes(guest.host, 18, 18) == 0 && FT_Set_Pixel_Sizes(native, 18, 18) == 0);
        Require(FT_Load_Glyph(guest.host, 1, FT_LOAD_DEFAULT) == 0 && FT_Load_Glyph(native, 1, FT_LOAD_DEFAULT) == 0);
        SyncFace(guest);
        FT_Outline_Reverse_nid_postfix(&guest.slot.outline);
        FT_Outline_Translate_nid_postfix(&guest.slot.outline, 77, -129);

        FT_Outline_Reverse(&native->glyph->outline);
        FT_Outline_Translate(&native->glyph->outline, 77, -129);
        Require(FT_Render_Glyph_nid_postfix(&guest.slot, FT_RENDER_MODE_NORMAL) == FT_Render_Glyph(native->glyph, FT_RENDER_MODE_NORMAL));
        const auto& actual = guest.slot.bitmap;
        const auto& expected = native->glyph->bitmap;
        Require(actual.width > 0 && actual.rows > 0 && actual.width == expected.width && actual.rows == expected.rows);
        Require(actual.pitch == expected.pitch && actual.pixel_mode == expected.pixel_mode && actual.num_grays == expected.num_grays);
        Require(guest.slot.bitmap_left == native->glyph->bitmap_left && guest.slot.bitmap_top == native->glyph->bitmap_top);
        const auto pitch = static_cast<std::size_t>(expected.pitch < 0 ? -expected.pitch : expected.pitch);
        Require(std::memcmp(actual.buffer, expected.buffer, pitch * expected.rows) == 0);
        Require(FT_Done_Face(guest.host) == 0 && FT_Done_Face(native) == 0);
    }
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}

int main() {
    FT_Outline_Reverse_nid_postfix(nullptr);
    FT_Outline_Translate_nid_postfix(nullptr, std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max());

    ReverseOracle(0, {}, FT_OUTLINE_REVERSE_FILL);
    for (std::size_t count = 1; count <= 8; ++count) {
        for (unsigned mask = 0; mask < (1u << (count - 1)); ++mask) {
            std::vector<std::uint16_t> contours;
            for (std::size_t index = 0; index < count; ++index) {
                if (index + 1 == count || (mask & (1u << index))) contours.push_back(static_cast<std::uint16_t>(index));
            }
            ReverseOracle(count, contours, static_cast<std::int32_t>(mask) | 0x31);
        }
    }
    ReverseOracle(65535, {65534}, 0);
    std::vector<std::uint16_t> maximumContours(65535);
    for (std::size_t index = 0; index < maximumContours.size(); ++index) maximumContours[index] = static_cast<std::uint16_t>(index);
    ReverseOracle(65535, maximumContours, FT_OUTLINE_REVERSE_FILL);
    for (std::size_t count = 0; count <= 6; ++count) {
        for (const FT_Pos xOffset : {-65, 0, 64, 127}) {
            for (const FT_Pos yOffset : {-99, 0, 101}) TranslateOracle(count, xOffset, yOffset);
        }
    }
    TranslateOracle(65535, 99, -17);
    WideTranslation();
    RenderModifiedOutline();
}
