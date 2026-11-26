#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <tuple>
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
#endif

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType outline render check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Span = std::tuple<int, int, int, int, int>;

static void APS5_VABI Collect(int y, int count, const FT_Span* spans, void* user) {
    auto* collected = static_cast<std::vector<Span>*>(user);
    for (int index = 0; index < count; ++index) collected->emplace_back(y, spans[index].x, spans[index].len, spans[index].coverage, 0);
}

static void NativeCollect(int y, int count, const FT_Span* spans, void* user) {
    Collect(y, count, spans, user);
}

struct Shape {
    std::vector<FT_Vector> points;
    std::vector<unsigned char> tags;
    std::vector<unsigned short> contours;
};

static Shape Square() {
    return {{{64, 64}, {576, 64}, {576, 576}, {64, 576}}, {1, 1, 1, 1}, {3}};
}

static Shape Triangle() {
    return {{{32, 32}, {672, 160}, {160, 672}}, {1, 1, 1}, {2}};
}

static Shape Curve() {
    return {{{64, 64}, {448, 64}, {704, 320}, {448, 576}, {64, 576}}, {1, 0, 1, 0, 1}, {4}};
}

static void CompareBitmap(GuestLibrary& library, FT_Library native, const Shape& shape, unsigned char mode) {
    std::vector<GuestVector> guestPoints;
    for (const FT_Vector& point : shape.points) guestPoints.push_back({point.x, point.y});
    std::vector<FT_Vector> nativePoints = shape.points;
    std::vector<unsigned char> tags = shape.tags;
    std::vector<unsigned short> contours = shape.contours;
    const auto count = static_cast<std::uint16_t>(shape.points.size());
    GuestOutline guest{1, count, guestPoints.data(), tags.data(), contours.data(), 0};
    FT_Outline expected{1, count, nativePoints.data(), tags.data(), contours.data(), 0};
    const std::int32_t pitch = mode == FT_PIXEL_MODE_MONO ? 2 : 12;
    std::vector<unsigned char> guestPixels(static_cast<std::size_t>(pitch) * 12, 0);
    std::vector<unsigned char> nativePixels = guestPixels;
    const GuestBitmap guestBitmap{12, 12, pitch, guestPixels.data(), static_cast<std::uint16_t>(mode == FT_PIXEL_MODE_MONO ? 2 : 256), mode, 0, nullptr};
    FT_Bitmap nativeBitmap{12, 12, pitch, nativePixels.data(), guestBitmap.num_grays, mode, 0, nullptr};
    GuestRasterParams params{};
    params.target = &guestBitmap;
    params.flags = mode == FT_PIXEL_MODE_MONO ? 0 : FT_RASTER_FLAG_AA;
    FT_Raster_Params nativeParams{};
    nativeParams.target = &nativeBitmap;
    nativeParams.flags = params.flags;
    Require(FT_Outline_Render_nid_postfix(&library, &guest, &params) == FT_Outline_Render(native, &expected, &nativeParams));
    Require(guestPixels == nativePixels);
    Require(params.source == &guest);
}

static void CompareNegativePitchBitmap(GuestLibrary& library, FT_Library native, const Shape& shape, unsigned char mode) {
    std::vector<GuestVector> guestPoints;
    for (const FT_Vector& point : shape.points) guestPoints.push_back({point.x, point.y});
    std::vector<FT_Vector> nativePoints = shape.points;
    std::vector<unsigned char> tags = shape.tags;
    std::vector<unsigned short> contours = shape.contours;
    const auto count = static_cast<std::uint16_t>(shape.points.size());
    GuestOutline guest{1, count, guestPoints.data(), tags.data(), contours.data(), 0};
    FT_Outline expected{1, count, nativePoints.data(), tags.data(), contours.data(), 0};
    const std::int32_t pitch = mode == FT_PIXEL_MODE_MONO ? 2 : 12;
    constexpr std::uint32_t rows = 12;
    constexpr std::size_t guard = 16;
    const std::size_t size = static_cast<std::size_t>(pitch) * rows;
    std::vector<unsigned char> guestPixels(guard + size + guard, 0xA5);
    std::fill(guestPixels.begin() + guard, guestPixels.begin() + guard + size, 0);
    std::vector<unsigned char> nativePixels = guestPixels;
    const std::vector<unsigned char> untouched = guestPixels;
    auto* guestBuffer = guestPixels.data() + guard;
    auto* nativeBuffer = nativePixels.data() + guard;
    const GuestBitmap guestBitmap{rows, 12, -pitch, guestBuffer, static_cast<std::uint16_t>(mode == FT_PIXEL_MODE_MONO ? 2 : 256), mode, 0, nullptr};
    FT_Bitmap nativeBitmap{rows, 12, -pitch, nativeBuffer, guestBitmap.num_grays, mode, 0, nullptr};
    GuestRasterParams params{};
    params.target = &guestBitmap;
    params.flags = mode == FT_PIXEL_MODE_MONO ? 0 : FT_RASTER_FLAG_AA;
    FT_Raster_Params nativeParams{};
    nativeParams.target = &nativeBitmap;
    nativeParams.flags = params.flags;
    Require(FT_Outline_Render_nid_postfix(&library, &guest, &params) == FT_Outline_Render(native, &expected, &nativeParams));
    Require(guestPixels == nativePixels);
    Require(params.source == &guest);
    const RasterTargetSpan tracked = RasterTargetSpan_nid_no_patch(guestBitmap);
    Require(tracked.begin == guestBuffer && tracked.bytes == size);
    bool written = false;
    for (std::size_t index = 0; index < guestPixels.size(); ++index) {
        if (guestPixels[index] == untouched[index]) continue;
        written = true;
        const unsigned char* at = guestPixels.data() + index;
        Require(at >= tracked.begin && at < tracked.begin + tracked.bytes);
    }
    Require(written);
}

static void CompareSpans(GuestLibrary& library, FT_Library native, const Shape& shape, bool clip) {
    std::vector<GuestVector> guestPoints;
    for (const FT_Vector& point : shape.points) guestPoints.push_back({point.x, point.y});
    std::vector<FT_Vector> nativePoints = shape.points;
    std::vector<unsigned char> tags = shape.tags;
    std::vector<unsigned short> contours = shape.contours;
    const auto count = static_cast<std::uint16_t>(shape.points.size());
    GuestOutline guest{1, count, guestPoints.data(), tags.data(), contours.data(), 0};
    FT_Outline expected{1, count, nativePoints.data(), tags.data(), contours.data(), 0};
    std::vector<Span> guestSpans;
    std::vector<Span> nativeSpans;
    GuestRasterParams params{};
    params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT | (clip ? FT_RASTER_FLAG_CLIP : 0);
    params.graySpans = reinterpret_cast<void*>(&Collect);
    params.user = &guestSpans;
    FT_Raster_Params nativeParams{};
    nativeParams.flags = params.flags;
    nativeParams.gray_spans = NativeCollect;
    nativeParams.user = &nativeSpans;
    if (clip) {
        params.clipBox = {1, 1, 6, 6};
        nativeParams.clip_box = {1, 1, 6, 6};
    }
    Require(FT_Outline_Render_nid_postfix(&library, &guest, &params) == FT_Outline_Render(native, &expected, &nativeParams));
    Require(!guestSpans.empty() && guestSpans == nativeSpans);
    Require(params.clipBox.xMin == nativeParams.clip_box.xMin && params.clipBox.yMin == nativeParams.clip_box.yMin);
    Require(params.clipBox.xMax == nativeParams.clip_box.xMax && params.clipBox.yMax == nativeParams.clip_box.yMax);
}

static void CheckInvalidOutlinePreservesOutputs(GuestLibrary& library, FT_Library native) {
    Shape shape = Square();
    shape.points[1].x = 0x1000001;
    shape.points[2].x = 0x1000001;
    std::vector<GuestVector> guestPoints;
    for (const FT_Vector& point : shape.points) guestPoints.push_back({point.x, point.y});
    FT_Outline nativeOutline{1, static_cast<unsigned short>(shape.points.size()), shape.points.data(), shape.tags.data(), shape.contours.data(), 0};
    GuestOutline guestOutline{1, static_cast<std::uint16_t>(guestPoints.size()), guestPoints.data(), shape.tags.data(), shape.contours.data(), 0};
    void* source = reinterpret_cast<void*>(static_cast<std::uintptr_t>(1));
    constexpr GuestBBox clip{11, 12, 13, 14};
    GuestRasterParams guestParams{};
    guestParams.flags = FT_RASTER_FLAG_DIRECT;
    guestParams.source = source;
    guestParams.clipBox = clip;
    FT_Raster_Params nativeParams{};
    nativeParams.flags = guestParams.flags;
    nativeParams.source = source;
    nativeParams.clip_box = {clip.xMin, clip.yMin, clip.xMax, clip.yMax};

    const FT_Error expected = FT_Outline_Render(native, &nativeOutline, &nativeParams);
    Require(expected != 0 && nativeParams.source == source);
    Require(nativeParams.clip_box.xMin == clip.xMin && nativeParams.clip_box.yMin == clip.yMin && nativeParams.clip_box.xMax == clip.xMax && nativeParams.clip_box.yMax == clip.yMax);
    Require(FT_Outline_Render_nid_postfix(&library, &guestOutline, &guestParams) == expected);
    Require(guestParams.source == source);
    Require(guestParams.clipBox.xMin == clip.xMin && guestParams.clipBox.yMin == clip.yMin && guestParams.clipBox.xMax == clip.xMax && guestParams.clipBox.yMax == clip.yMax);
}

template <typename TValue>
static TValue* ReadOnlyCopy(const TValue& value) {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, sizeof(value), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(page != nullptr);
    std::memcpy(page, &value, sizeof(value));
    DWORD previous = 0;
    Require(VirtualProtect(page, sizeof(value), PAGE_READONLY, &previous) != 0);
#else
    void* page = mmap(nullptr, sizeof(value), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
    std::memcpy(page, &value, sizeof(value));
    Require(mprotect(page, sizeof(value), PROT_READ) == 0);
#endif
    return static_cast<TValue*>(page);
}

static void CheckUnusableGuestPointers(GuestLibrary& library, FT_Library native) {
    Shape square = Square();
    std::vector<GuestVector> points;
    for (const FT_Vector& point : square.points) points.push_back({point.x, point.y});
    GuestOutline outline{1, 4, points.data(), square.tags.data(), square.contours.data(), 0};
    FT_Outline nativeOutline{1, 4, square.points.data(), square.tags.data(), square.contours.data(), 0};
    const FT_Error noOutline = FT_Outline_Render(native, nullptr, nullptr);
    const FT_Error noParams = FT_Outline_Render(native, &nativeOutline, nullptr);
    Require(noOutline != 0 && noParams != 0);

    errno = 71;
    Require(FT_Outline_Render_nid_postfix(&library, reinterpret_cast<GuestOutline*>(0x10), nullptr) == noOutline);
    Require(errno == 71);
    GuestOutline unmappedPoints = outline;
    unmappedPoints.points = reinterpret_cast<GuestVector*>(0x10);
    errno = 71;
    Require(FT_Outline_Render_nid_postfix(&library, &unmappedPoints, nullptr) == noOutline);
    Require(errno == 71);
    GuestOutline unmappedContours = outline;
    unmappedContours.contours = reinterpret_cast<std::uint16_t*>(0x10);
    errno = 71;
    Require(FT_Outline_Render_nid_postfix(&library, &unmappedContours, nullptr) == noOutline);
    Require(errno == 71);

    errno = 71;
    Require(FT_Outline_Render_nid_postfix(&library, &outline, reinterpret_cast<GuestRasterParams*>(0x10)) == noParams);
    Require(errno == 71);

    std::vector<Span> spans;
    GuestRasterParams direct{};
    direct.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
    direct.graySpans = reinterpret_cast<void*>(&Collect);
    direct.user = &spans;
    GuestRasterParams* readOnly = ReadOnlyCopy(direct);
    errno = 71;
    Require(FT_Outline_Render_nid_postfix(&library, &outline, readOnly) == FT_Err_Invalid_Argument);
    Require(errno == 71 && readOnly->source == nullptr && spans.empty());
    const FT_Error noLibrary = FT_Outline_Render(nullptr, &nativeOutline, nullptr);
    Require(noLibrary == FT_Err_Invalid_Library_Handle);
    GuestLibrary emptyLibrary{};
    for (GuestLibrary* unusable : {reinterpret_cast<GuestLibrary*>(0x10), reinterpret_cast<GuestLibrary*>(reinterpret_cast<std::uintptr_t>(&library) + 1), &emptyLibrary}) {
        errno = 71;
        Require(FT_Outline_Render_nid_postfix(unusable, &outline, readOnly) == noLibrary);
        Require(errno == 71 && readOnly->source == nullptr && spans.empty());
    }

    GuestRasterParams unmappedTarget{};
    unmappedTarget.flags = FT_RASTER_FLAG_AA;
    unmappedTarget.target = reinterpret_cast<const GuestBitmap*>(0x10);
    errno = 71;
    Require(FT_Outline_Render_nid_postfix(&library, &outline, &unmappedTarget) == FT_Err_Invalid_Argument);
    Require(errno == 71 && unmappedTarget.source == nullptr);
}

int main() {
    GuestLibrary library{};
    FT_Library native = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
    CheckInvalidOutlinePreservesOutputs(library, native);
    CheckUnusableGuestPointers(library, native);
    for (const auto& make : {+[] { return Square(); }, +[] { return Triangle(); }, +[] { return Curve(); }}) {
        for (const unsigned char mode : {static_cast<unsigned char>(FT_PIXEL_MODE_GRAY), static_cast<unsigned char>(FT_PIXEL_MODE_MONO)}) CompareBitmap(library, native, make(), mode);
        for (const unsigned char mode : {static_cast<unsigned char>(FT_PIXEL_MODE_GRAY), static_cast<unsigned char>(FT_PIXEL_MODE_MONO)}) CompareNegativePitchBitmap(library, native, make(), mode);
        CompareSpans(library, native, make(), false);
        CompareSpans(library, native, make(), true);
    }
    Shape square = Square();
    std::vector<GuestVector> points;
    for (const FT_Vector& point : square.points) points.push_back({point.x, point.y});
    GuestOutline outline{1, 4, points.data(), square.tags.data(), square.contours.data(), 0};
    FT_Outline nativeOutline{1, 4, square.points.data(), square.tags.data(), square.contours.data(), 0};
    GuestRasterParams params{};
    FT_Raster_Params nativeParams{};
    Require(FT_Outline_Render_nid_postfix(&library, &outline, nullptr) == FT_Outline_Render(native, &nativeOutline, nullptr));
    Require(FT_Outline_Render_nid_postfix(&library, nullptr, &params) == FT_Outline_Render(native, nullptr, &nativeParams));
    Require(FT_Outline_Render_nid_postfix(nullptr, &outline, &params) == FT_Outline_Render(nullptr, &nativeOutline, &nativeParams));
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
}
