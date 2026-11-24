#include "prx/libSceFreeTypeOt/include/SizeMetrics.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/internal/ftobjs.h>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
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

template <typename TCall> bool Invalid(TCall call) {
    errno = 71;
    try {
        call();
    } catch (const std::invalid_argument&) {
        return errno == 71;
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

struct NativeMatchResult {
    std::int32_t error;
    std::uint64_t index;
};

NativeMatchResult NativeMatch(const Fixture& fixture, const GuestSizeRequestRec& request, unsigned char ignoreWidth, std::uint64_t initialIndex) {
    std::vector<FT_Bitmap_Size> strikes;
    for (std::int32_t index = 0; index < fixture.face.num_fixed_sizes; ++index) {
        const GuestBitmapSize& strike = fixture.face.available_sizes[index];
        strikes.push_back({strike.height, strike.width, static_cast<FT_Pos>(strike.size), static_cast<FT_Pos>(strike.x_ppem), static_cast<FT_Pos>(strike.y_ppem)});
    }
    FT_FaceRec face{};
    face.face_flags = static_cast<FT_Long>(fixture.face.face_flags);
    face.num_fixed_sizes = fixture.face.num_fixed_sizes;
    face.available_sizes = strikes.data();
    FT_Size_RequestRec hostRequest{static_cast<FT_Size_Request_Type>(request.type), static_cast<FT_Long>(request.width), static_cast<FT_Long>(request.height),
        request.horiResolution, request.vertResolution};
    FT_ULong index = static_cast<FT_ULong>(initialIndex);
    const FT_Error error = FT_Match_Size(&face, &hostRequest, ignoreWidth, &index);
    return {error, index};
}

void RequireNativeAgreement(Fixture& fixture, const GuestSizeRequestRec& request, unsigned char ignoreWidth, const char* message) {
    constexpr std::uint64_t InitialIndex = 99;
    const NativeMatchResult native = NativeMatch(fixture, request, ignoreWidth, InitialIndex);
    std::uint64_t guestIndex = InitialIndex;
    const std::int32_t guestError = FT_Match_Size_nid_postfix(&fixture.face, &request, ignoreWidth, &guestIndex);
    Require(guestError == native.error && guestIndex == native.index, message);
}

void RequestMetrics() {
    Require(Equals(Request(0, 16 * 64, 16 * 64, 0, 0), 16, 16, 32768, 32768, 960, -256, 1216, 1280), "nominal request grid-fits the scaled metrics");
    Require(Equals(Request(0, 12 * 64, 0, 96, 96), 16, 16, 32768, 32768, 960, -256, 1216, 1280), "nominal request applies the resolution and copies the width scale");
    Require(Equals(Request(1, 0, 20 * 64, 0, 0), 17, 17, 35187, 35187, 1024, -320, 1280, 1344), "real-dimension request scales ascender minus descender");
    Require(Equals(Request(2, 30 * 64, 10 * 64, 72, 72), 15, 8, 31457, 16132, 512, -128, 576, 1216), "bbox request scales each axis by the bbox");
    Require(Equals(Request(3, 10 * 64, 40 * 64, 0, 0), 8, 8, 16777, 16777, 512, -128, 640, 640), "cell request keeps the smaller scale");
    Require(Equals(Request(4, 0x8000, 0, 0, 0), 16, 16, 32768, 32768, 960, -256, 1216, 1280), "scales request copies the given scale to the missing axis");
    Require(Equals(Request(4, 0, 0x8000, 0, 0), 16, 16, 32768, 32768, 960, -256, 1216, 1280), "scales request copies the given height scale to the missing axis");
    Require(Equals(Request(0, -16 * 64, 8 * 64, 0, 0), 65520, 8, -32768, 16384, 512, -128, 576, -1280), "negative widths keep their sign and truncate the ppem");
    const auto resolvedNegative = Request(0, -12 * 64, 12 * 64, 96, 96);
    Require(resolvedNegative.x_scale == -32736 && resolvedNegative.y_scale == 32768, "negative resolved sizes truncate before scaling");
    if constexpr (sizeof(FT_Long) == sizeof(std::int32_t)) {
        Require(Invalid([] { Request(0, std::int64_t{1} << 30, std::int64_t{1} << 30, 300, 300); }), "resolved sizes outside FT_Long throw");
    }
    Require(Equals(Request(0, 0, 0, 0, 0), 0, 0, 0, 0, 0, 0, 0, 0), "zero dimensions clear scalable metrics");

    for (const std::uint32_t type : {std::uint32_t{5}, std::numeric_limits<std::uint32_t>::max()}) {
        Fixture unsupported(1);
        const GuestSizeMetrics unsupportedBefore = unsupported.size.metrics;
        const GuestSizeRequestRec unsupportedRequest{type, 16 * 64, 16 * 64, 0, 0};
        Require(FT_Request_Metrics_nid_postfix(&unsupported.face, &unsupportedRequest) == 0x85, "an unsupported request type divides by zero");
        Require(std::memcmp(&unsupportedBefore, &unsupported.size.metrics, sizeof(unsupportedBefore)) == 0,
                "an unsupported request type leaves the metrics untouched");
    }

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

    Fixture zeroWidth(1);
    zeroWidth.face.bbox.xMin = zeroWidth.face.bbox.xMax;
    const GuestSizeMetrics zeroWidthBefore = zeroWidth.size.metrics;
    const GuestSizeRequestRec bbox{2, 16 * 64, 16 * 64, 0, 0};
    Require(FT_Request_Metrics_nid_postfix(&zeroWidth.face, &bbox) == 0x85, "a zero requested face width is a division by zero");
    Require(zeroWidth.size.metrics.x_scale == zeroWidthBefore.x_scale && zeroWidth.size.metrics.y_scale != zeroWidthBefore.y_scale,
            "a width division failure keeps the preceding height scale update");

    Fixture oversized(1);
    const GuestSizeMetrics oversizedBefore = oversized.size.metrics;
    const GuestSizeRequestRec scales{4, 0x7FFFFFFF, 0x7FFFFFFF, 0, 0};
    Require(FT_Request_Metrics_nid_postfix(&oversized.face, &scales) == 0x17, "an oversized ppem is an invalid pixel size");
    Require(oversized.size.metrics.x_ppem == oversizedBefore.x_ppem && oversized.size.metrics.y_ppem == oversizedBefore.y_ppem &&
                oversized.size.metrics.x_scale == scales.width && oversized.size.metrics.y_scale == scales.height,
            "an invalid pixel size keeps the scale updates and old ppems");

    Fixture scalable(1);
    Require(Invalid([&] { FT_Request_Metrics_nid_postfix(&scalable.face, nullptr); }), "a null request on a scalable face throws");
    Require(Invalid([] { FT_Request_Metrics_nid_postfix(nullptr, nullptr); }), "a null face throws");
}

void SelectMetrics() {
    Fixture scalable(3);
    FT_Select_Metrics_nid_postfix(&scalable.face, 1);
    Require(Equals(scalable.size.metrics, 16, 16, 32768, 32768, 960, -256, 1216, 1280), "a scalable face derives the scales from the strike");
    Fixture bitmap(2);
    FT_Select_Metrics_nid_postfix(&bitmap.face, 2);
    Require(Equals(bitmap.size.metrics, 18, 16, 65536, 65536, 1024, 0, 1024, 1152), "a bitmap face copies the strike metrics");
    Require(Invalid([&] { FT_Select_Metrics_nid_postfix(&bitmap.face, 3); }), "a strike index past the table throws");

    Fixture missingTable(3);
    missingTable.face.available_sizes = nullptr;
    Require(Invalid([&] { FT_Select_Metrics_nid_postfix(&missingTable.face, 0); }), "a missing strike table throws");
    Require(Invalid([] { FT_Select_Metrics_nid_postfix(nullptr, 0); }), "a null face throws");
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
    Require(FT_Match_Size_nid_postfix(&fixture.face, &request, 0, reinterpret_cast<std::uint64_t*>(0x10)) == 0x06, "an unwritable size index is refused");
    request = {3, 16 * 64, 16 * 64, 0, 0};
    index = 99;
    Require(FT_Match_Size_nid_postfix(&fixture.face, &request, 0, &index) == 0x07, "only nominal requests are matched");
    Require(index == 99, "a rejected request leaves the index untouched");
    Fixture outlineOnly(1);
    Require(FT_Match_Size_nid_postfix(&outlineOnly.face, nullptr, 0, &index) == 0x23, "a face without strikes is an invalid face handle");

    request = {0, 20 * 64, 16 * 64, 0, 0};
    Require(FT_Match_Size_nid_postfix(&fixture.face, &request, 0, reinterpret_cast<std::uint64_t*>(0x10)) == 0x17,
            "an unused invalid output does not replace a no-match error");

    request = {0, std::numeric_limits<std::int64_t>::max(), 16 * 64, 0, 0};
    index = 99;
    Require(Throws([&] { FT_Match_Size_nid_postfix(&fixture.face, &request, 0, &index); }) && index == 99,
            "request rounding overflow throws without writing the index");
    request = {0, std::numeric_limits<std::int64_t>::max() / 2 + 1, 16 * 64, 2, 0};
    Require(Throws([&] { FT_Match_Size_nid_postfix(&fixture.face, &request, 0, &index); }) && index == 99,
            "resolution multiplication overflow throws without writing the index");
    fixture.sizes[0].x_ppem = std::numeric_limits<std::int64_t>::max();
    fixture.sizes[0].y_ppem = 16 * 64;
    request = {0, 16 * 64, 16 * 64, 0, 0};
    Require(Throws([&] { FT_Match_Size_nid_postfix(&fixture.face, &request, 1, &index); }) && index == 99,
            "strike rounding overflow throws before ignored-width matching");
}

void MatchNativeAgreement() {
    Fixture fixture(3);
    const std::array<GuestSizeRequestRec, 9> requests = {{{0, 16 * 64, 16 * 64, 0, 0}, {0, 18 * 64, 16 * 64, 0, 0},
        {0, 0, 13 * 64 - 20, 0, 0}, {0, 12, 0, 96 * 64, 0}, {0, 0, 0, 0, 0}, {1, 16 * 64, 16 * 64, 0, 0},
        {2, 16 * 64, 16 * 64, 0, 0}, {3, 16 * 64, 16 * 64, 0, 0}, {4, 16 * 64, 16 * 64, 0, 0}}};
    for (const GuestSizeRequestRec& request : requests) {
        RequireNativeAgreement(fixture, request, 0, "guest matching agrees with pinned FreeType for every request type");
    }
    const GuestSizeRequestRec ignoredWidth{0, 20 * 64, 16 * 64, 0, 0};
    RequireNativeAgreement(fixture, ignoredWidth, 1, "guest ignored-width matching agrees with pinned FreeType");

    fixture.sizes[0].x_ppem = 16 * 64;
    fixture.sizes[0].y_ppem = 16 * 64;
    const GuestSizeRequestRec tied{0, 16 * 64, 16 * 64, 0, 0};
    RequireNativeAgreement(fixture, tied, 0, "guest tie selection agrees with pinned FreeType");

    Fixture empty(3);
    empty.face.num_fixed_sizes = 0;
    empty.face.available_sizes = nullptr;
    RequireNativeAgreement(empty, tied, 0, "guest empty-strike matching agrees with pinned FreeType");

    Fixture scalable(1);
    RequireNativeAgreement(scalable, tied, 0, "guest scalable-face rejection agrees with pinned FreeType");
}

template <typename TCall> bool InvalidArgument(TCall call) {
    errno = 71;
    return call() == 0x06 && errno == 71;
}

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(base != nullptr, "the read-only page is allocated");
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(base != MAP_FAILED, "the read-only page is allocated");
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    void Protect() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(base, size, PAGE_READONLY, &previous) != 0, "the page becomes read-only");
#else
        Require(mprotect(base, size, PROT_READ) == 0, "the page becomes read-only");
#endif
    }

    void* base = nullptr;
    std::size_t size = 0;
};

void GuestPointers() {
    auto* unmappedFace = reinterpret_cast<GuestFaceRec*>(std::uintptr_t{0x10});
    const auto* unmappedRequest = reinterpret_cast<const GuestSizeRequestRec*>(std::uintptr_t{0x10});
    const GuestSizeRequestRec request{0, 16 * 64, 16 * 64, 0, 0};
    std::uint64_t index = 99;
    Require(Invalid([&] { FT_Select_Metrics_nid_postfix(unmappedFace, 0); }), "an unmapped face throws from FT_Select_Metrics");
    Require(InvalidArgument([&] { return FT_Request_Metrics_nid_postfix(unmappedFace, &request); }), "an unmapped face is Invalid_Argument for FT_Request_Metrics");
    Require(InvalidArgument([&] { return FT_Match_Size_nid_postfix(unmappedFace, &request, 0, &index); }) && index == 99,
            "an unmapped face is Invalid_Argument for FT_Match_Size");

    Fixture scalable(1);
    const GuestSizeMetrics scalableBefore = scalable.size.metrics;
    Require(InvalidArgument([&] { return FT_Request_Metrics_nid_postfix(&scalable.face, unmappedRequest); }), "an unmapped request is Invalid_Argument for FT_Request_Metrics");
    Require(std::memcmp(&scalableBefore, &scalable.size.metrics, sizeof(scalableBefore)) == 0, "an unmapped request leaves the metrics untouched");
    Fixture fixed(3);
    Require(InvalidArgument([&] { return FT_Match_Size_nid_postfix(&fixed.face, unmappedRequest, 0, &index); }) && index == 99,
            "an unmapped request is Invalid_Argument for FT_Match_Size");

    Fixture unmappedSize(3);
    unmappedSize.face.size = reinterpret_cast<GuestSizeRec*>(std::uintptr_t{0x10});
    Require(Invalid([&] { FT_Select_Metrics_nid_postfix(&unmappedSize.face, 1); }), "an unmapped size throws from FT_Select_Metrics");
    Require(InvalidArgument([&] { return FT_Request_Metrics_nid_postfix(&unmappedSize.face, &request); }), "an unmapped size is Invalid_Argument for FT_Request_Metrics");

    Fixture unmappedStrikes(3);
    unmappedStrikes.face.available_sizes = reinterpret_cast<GuestBitmapSize*>(std::uintptr_t{0x10});
    Require(FT_Request_Metrics_nid_postfix(&unmappedStrikes.face, &request) == 0, "FT_Request_Metrics does not read bitmap strikes");
    const GuestSizeMetrics strikesBefore = unmappedStrikes.size.metrics;
    Require(Invalid([&] { FT_Select_Metrics_nid_postfix(&unmappedStrikes.face, 1); }), "unmapped strikes throw from FT_Select_Metrics");
    Require(InvalidArgument([&] { return FT_Match_Size_nid_postfix(&unmappedStrikes.face, &request, 0, &index); }) && index == 99,
            "unmapped strikes are Invalid_Argument for FT_Match_Size");
    Require(std::memcmp(&strikesBefore, &unmappedStrikes.size.metrics, sizeof(strikesBefore)) == 0, "unmapped strikes leave the metrics untouched");
    unmappedStrikes.face.num_fixed_sizes = std::numeric_limits<std::int32_t>::max();
    Require(InvalidArgument([&] { return FT_Match_Size_nid_postfix(&unmappedStrikes.face, &request, 0, &index); }) && index == 99,
            "a huge strike count over unmapped strikes is Invalid_Argument");

    ReadOnlyPage page;
    Fixture readOnly(3);
    auto* size = new (page.base) GuestSizeRec(readOnly.size);
    readOnly.face.size = size;
    const GuestSizeMetrics readOnlyBefore = size->metrics;
    page.Protect();
    Require(Invalid([&] { FT_Select_Metrics_nid_postfix(&readOnly.face, 1); }), "a read-only size throws from FT_Select_Metrics");
    Require(InvalidArgument([&] { return FT_Request_Metrics_nid_postfix(&readOnly.face, &request); }), "a read-only size is Invalid_Argument for FT_Request_Metrics");
    Require(std::memcmp(&readOnlyBefore, &size->metrics, sizeof(readOnlyBefore)) == 0, "a read-only size keeps its metrics");
}

}

int main() {
    RequestMetrics();
    SelectMetrics();
    MatchSize();
    MatchNativeAgreement();
    GuestPointers();
    std::puts("Guest FreeType Ot size metrics tests passed");
    return 0;
}
