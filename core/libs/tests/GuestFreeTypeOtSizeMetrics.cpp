#include "prx/libSceFreeTypeOt/include/SizeMetrics.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

using FreeType::GuestBitmapSize;
using FreeType::GuestFaceRec;
using FreeType::GuestSizeMetrics;
using FreeType::GuestSizeRec;
using FreeType::GuestSizeRequestRec;

struct Fixture {
    GuestFaceRec face{};
    GuestSizeRec size{};
    GuestBitmapSize sizes[3] = {{13, 7, 13 * 64, 13 * 64, 13 * 64}, {16, 8, 16 * 64, 16 * 64, 16 * 64}, {16, 9, 16 * 64, 18 * 64, 16 * 64}};

    explicit Fixture(std::int64_t flags) {
        std::memset(&size, 0x5A, sizeof(size));
        // The metrics are copied into a host FT_Long, which is 32-bit on Windows; keep the sentinels inside it.
        size.metrics.x_scale = size.metrics.y_scale = size.metrics.ascender = 0x5A5A5A5A;
        size.metrics.descender = size.metrics.height = size.metrics.max_advance = 0x5A5A5A5A;
        face.size = &size;
        face.face_flags = flags;
        face.units_per_EM = 2048;
        face.ascender = 1901;
        face.descender = -483;
        face.height = 2384;
        face.max_advance_width = 2500;
        face.bbox = {-1000, -600, 3000, 2000};
        face.num_fixed_sizes = 3;
        face.available_sizes = sizes;
    }
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot size metrics: %s\n", message);
    std::exit(1);
}

template <typename TCall> bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool Equals(const GuestSizeMetrics& m, std::uint16_t xPpem, std::uint16_t yPpem, std::int64_t xScale, std::int64_t yScale, std::int64_t ascender, std::int64_t descender, std::int64_t height, std::int64_t maxAdvance) {
    return m.x_ppem == xPpem && m.y_ppem == yPpem && m.x_scale == xScale && m.y_scale == yScale && m.ascender == ascender && m.descender == descender &&
        m.height == height && m.max_advance == maxAdvance;
}

GuestSizeMetrics Request(std::uint32_t type, std::int64_t width, std::int64_t height, std::uint32_t hori, std::uint32_t vert) {
    Fixture fixture(1);
    const GuestSizeRequestRec request{type, width, height, hori, vert};
    Require(FT_Request_Metrics_nid_postfix(&fixture.face, &request) == 0, "a valid request succeeds");
    return fixture.size.metrics;
}

void RequestMetrics() {
    Require(Equals(Request(0, 16 * 64, 16 * 64, 0, 0), 16, 16, 32768, 32768, 960, -256, 1216, 1280), "nominal request grid-fits the scaled metrics");
    Require(Equals(Request(0, 12 * 64, 0, 96, 96), 16, 16, 32768, 32768, 960, -256, 1216, 1280), "nominal request applies the resolution and copies the width scale");
    Require(Equals(Request(1, 0, 20 * 64, 0, 0), 17, 17, 35187, 35187, 1024, -320, 1280, 1344), "real-dimension request scales ascender minus descender");
    Require(Equals(Request(2, 30 * 64, 10 * 64, 72, 72), 15, 8, 31457, 16132, 512, -128, 576, 1216), "bbox request scales each axis by the bbox");
    Require(Equals(Request(3, 10 * 64, 40 * 64, 0, 0), 8, 8, 16777, 16777, 512, -128, 640, 640), "cell request keeps the smaller scale");
    Require(Equals(Request(4, 0x8000, 0, 0, 0), 16, 16, 32768, 32768, 960, -256, 1216, 1280), "scales request copies the given scale to the missing axis");
    Require(Equals(Request(0, -16 * 64, 8 * 64, 0, 0), 65520, 8, -32768, 16384, 512, -128, 576, -1280), "negative widths keep their sign and truncate the ppem");

    Fixture bitmapOnly(2);
    const GuestSizeRequestRec request{0, 16 * 64, 16 * 64, 0, 0};
    FT_Request_Metrics_nid_postfix(&bitmapOnly.face, &request);
    Require(Equals(bitmapOnly.size.metrics, 0, 0, 65536, 65536, 0, 0, 0, 0), "a face without outlines gets unit scales and zero metrics");
    Fixture noRequest(2);
    FT_Request_Metrics_nid_postfix(&noRequest.face, nullptr);
    Require(Equals(noRequest.size.metrics, 0, 0, 65536, 65536, 0, 0, 0, 0), "a face without outlines does not read the request");

    Fixture flat(1);
    flat.face.ascender = 0;
    flat.face.descender = 0;
    const GuestSizeMetrics before = flat.size.metrics;
    const GuestSizeRequestRec realDim{1, 0, 20 * 64, 0, 0};
    Require(FT_Request_Metrics_nid_postfix(&flat.face, &realDim) == 0x85, "a zero face dimension is a division by zero");
    Require(std::memcmp(&before, &flat.size.metrics, sizeof(before)) == 0, "a division by zero leaves the metrics untouched");

    Fixture scalable(1);
    Require(Throws([&] { FT_Request_Metrics_nid_postfix(&scalable.face, nullptr); }), "a null request on a scalable face throws");
    Require(Throws([] { FT_Request_Metrics_nid_postfix(nullptr, nullptr); }), "a null face throws");
}

void SelectMetrics() {
    Fixture scalable(3);
    FT_Select_Metrics_nid_postfix(&scalable.face, 1);
    Require(Equals(scalable.size.metrics, 16, 16, 32768, 32768, 960, -256, 1216, 1280), "a scalable face derives the scales from the strike");
    Fixture bitmap(2);
    FT_Select_Metrics_nid_postfix(&bitmap.face, 2);
    Require(Equals(bitmap.size.metrics, 18, 16, 65536, 65536, 1024, 0, 1024, 1152), "a bitmap face copies the strike metrics");
    Require(Throws([&] { FT_Select_Metrics_nid_postfix(&bitmap.face, 3); }), "a strike index past the table throws");
}

void MatchSize() {
    Fixture fixture(3);
    GuestSizeRequestRec request{0, 16 * 64, 16 * 64, 0, 0};
    std::uint64_t index = 99;
    Require(FT_Match_Size_nid_postfix(&fixture.face, &request, 0, &index) == 0 && index == 1, "the first strike with both ppems matches");
    request.width = 18 * 64;
    Require(FT_Match_Size_nid_postfix(&fixture.face, &request, 0, &index) == 0 && index == 2, "the width selects between strikes of the same height");
    request.width = 20 * 64;
    index = 99;
    Require(FT_Match_Size_nid_postfix(&fixture.face, &request, 0, &index) == 0x17 && index == 99, "no matching strike is an invalid pixel size");
    Require(FT_Match_Size_nid_postfix(&fixture.face, &request, 1, &index) == 0 && index == 1, "ignoring the width matches the height only");
    request = {0, 0, 13 * 64 - 20, 0, 0};
    Require(FT_Match_Size_nid_postfix(&fixture.face, &request, 0, &index) == 0 && index == 0, "a missing width takes the height and both round to pixels");
    request = {0, 12, 0, 96 * 64, 0};
    Require(FT_Match_Size_nid_postfix(&fixture.face, &request, 0, nullptr) == 0, "the resolution scales the request and the index is optional");
    request = {3, 16 * 64, 16 * 64, 0, 0};
    Require(FT_Match_Size_nid_postfix(&fixture.face, &request, 0, &index) == 0x07, "only nominal requests are matched");
    Fixture outlineOnly(1);
    Require(FT_Match_Size_nid_postfix(&outlineOnly.face, nullptr, 0, &index) == 0x23, "a face without strikes is an invalid face handle");
}

}

int main() {
    RequestMetrics();
    SelectMetrics();
    MatchSize();
    std::puts("Guest FreeType Ot size metrics tests passed");
    return 0;
}
