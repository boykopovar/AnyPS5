#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_STROKER_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace FreeType;

namespace {

constexpr bool SharedPoints = sizeof(FT_Pos) == sizeof(std::int64_t);

static_assert(!SharedPoints || (sizeof(FT_Vector) == sizeof(GuestVector) && offsetof(FT_Vector, x) == offsetof(GuestVector, x) &&
                                offsetof(FT_Vector, y) == offsetof(GuestVector, y)));

FT_Pos HostCoordinate(std::int64_t value, const char* function) {
    if (value < std::numeric_limits<FT_Pos>::min() || value > std::numeric_limits<FT_Pos>::max()) {
        throw std::runtime_error(std::string(function) + ": a value outside the host FT_Pos range is not implemented");
    }
    return static_cast<FT_Pos>(value);
}

struct NativeVector {
    FT_Vector value{};
    FT_Vector* pointer = nullptr;

    NativeVector(const GuestVector* guest, const char* function) {
        if (!guest) return;
        value = {HostCoordinate(guest->x, function), HostCoordinate(guest->y, function)};
        pointer = &value;
    }
};

void WriteCount(std::uint32_t* target, FT_UInt value) {
    if (!target) return;
    GuestArena::HostWrite write(target, sizeof(*target));
    *target = value;
}

FT_Outline NativeOutline(const GuestOutline& outline, FT_Vector* points) {
    return {outline.n_contours, outline.n_points, points, outline.tags, outline.contours, outline.flags};
}

bool FitsInt32(std::int64_t value) {
    return value >= std::numeric_limits<std::int32_t>::min() && value <= std::numeric_limits<std::int32_t>::max();
}

void CheckOutlineArrays(const GuestOutline& outline, const char* function) {
    if (outline.n_contours == 0) return;
    if (!outline.points || !outline.tags || !outline.contours) NotImplemented_nid_no_patch(function);
    for (std::size_t contour = 0; contour < outline.n_contours; ++contour) {
        if (outline.contours[contour] >= outline.n_points) NotImplemented_nid_no_patch(function);
    }
}

bool StrokedPointsFitInt32(const GuestOutline& outline) {
    std::int32_t last = -1;
    for (std::size_t contour = 0; contour < outline.n_contours; ++contour) {
        const std::int32_t first = last + 1;
        last = outline.contours[contour];
        if (last <= first) continue;
        for (std::int32_t index = first; index <= last; ++index) {
            if (!FitsInt32(outline.points[index].x) || !FitsInt32(outline.points[index].y)) return false;
        }
    }
    return true;
}

std::vector<FT_Vector> NativePoints(const GuestOutline& outline, const char* function) {
    std::vector<FT_Vector> points;
    if (outline.n_contours == 0) return points;
    points.reserve(outline.n_points);
    for (std::size_t index = 0; index < outline.n_points; ++index) {
        points.push_back({HostCoordinate(outline.points[index].x, function), HostCoordinate(outline.points[index].y, function)});
    }
    return points;
}

void ExportBorder(void* stroker, std::int32_t border, GuestOutline* outline, const char* function) {
    if (!stroker || !outline || (border != FT_STROKER_BORDER_LEFT && border != FT_STROKER_BORDER_RIGHT)) return;
    if constexpr (SharedPoints) {
        FT_Outline native = NativeOutline(*outline, reinterpret_cast<FT_Vector*>(outline->points));
        FT_Stroker_ExportBorder(static_cast<FT_Stroker>(stroker), static_cast<FT_StrokerBorder>(border), &native);
        outline->n_contours = native.n_contours;
        outline->n_points = native.n_points;
    } else {
        NotImplemented_nid_no_patch(function);
    }
}

}

extern "C" {

int APS5_VABI FT_Stroker_New_nid_postfix(GuestLibrary* library, void** stroker) {
    FT_Stroker native = nullptr;
    const FT_Error error = FT_Stroker_New(library ? library->host : nullptr, stroker ? &native : nullptr);
    if (stroker) *stroker = native;
    return error;
}

void APS5_VABI FT_Stroker_Done_nid_postfix(void* stroker) {
    FT_Stroker_Done(static_cast<FT_Stroker>(stroker));
}

int APS5_VABI FT_Stroker_GetCounts_nid_postfix(void* stroker, std::uint32_t* points, std::uint32_t* contours) {
    FT_UInt nativePoints = 0;
    FT_UInt nativeContours = 0;
    const FT_Error error = FT_Stroker_GetCounts(static_cast<FT_Stroker>(stroker), points ? &nativePoints : nullptr, contours ? &nativeContours : nullptr);
    WriteCount(points, nativePoints);
    WriteCount(contours, nativeContours);
    return error;
}

int APS5_VABI FT_Stroker_BeginSubPath_nid_postfix(void* stroker, const GuestVector* to, std::uint8_t open) {
    NativeVector vector(to, __func__);
    return FT_Stroker_BeginSubPath(static_cast<FT_Stroker>(stroker), vector.pointer, open);
}

int APS5_VABI FT_Stroker_LineTo_nid_postfix(void* stroker, const GuestVector* to) {
    NativeVector vector(to, __func__);
    return FT_Stroker_LineTo(static_cast<FT_Stroker>(stroker), vector.pointer);
}

int APS5_VABI FT_Stroker_EndSubPath_nid_postfix(void* stroker) {
    return FT_Stroker_EndSubPath(static_cast<FT_Stroker>(stroker));
}

void APS5_VABI FT_Stroker_Set_nid_postfix(void* stroker, std::int64_t radius, std::int32_t lineCap, std::int32_t lineJoin, std::int64_t miterLimit) {
    FT_Stroker_Set(static_cast<FT_Stroker>(stroker), HostCoordinate(radius, __func__), static_cast<FT_Stroker_LineCap>(lineCap),
                   static_cast<FT_Stroker_LineJoin>(lineJoin), HostCoordinate(miterLimit, __func__));
}

void APS5_VABI FT_Stroker_Rewind_nid_postfix(void* stroker) {
    FT_Stroker_Rewind(static_cast<FT_Stroker>(stroker));
}

int APS5_VABI FT_Stroker_GetBorderCounts_nid_postfix(void* stroker, std::int32_t border, std::uint32_t* points, std::uint32_t* contours) {
    FT_UInt nativePoints = 0;
    FT_UInt nativeContours = 0;
    const FT_Error error = FT_Stroker_GetBorderCounts(static_cast<FT_Stroker>(stroker), static_cast<FT_StrokerBorder>(border),
                                                      points ? &nativePoints : nullptr, contours ? &nativeContours : nullptr);
    WriteCount(points, nativePoints);
    WriteCount(contours, nativeContours);
    return error;
}

int APS5_VABI FT_Stroker_ConicTo_nid_postfix(void* stroker, const GuestVector* control, const GuestVector* to) {
    NativeVector nativeControl(control, __func__);
    NativeVector nativeTo(to, __func__);
    return FT_Stroker_ConicTo(static_cast<FT_Stroker>(stroker), nativeControl.pointer, nativeTo.pointer);
}

int APS5_VABI FT_Stroker_CubicTo_nid_postfix(void* stroker, const GuestVector* control1, const GuestVector* control2, const GuestVector* to) {
    NativeVector first(control1, __func__);
    NativeVector second(control2, __func__);
    NativeVector end(to, __func__);
    return FT_Stroker_CubicTo(static_cast<FT_Stroker>(stroker), first.pointer, second.pointer, end.pointer);
}

int APS5_VABI FT_Stroker_ParseOutline_nid_postfix(void* stroker, const GuestOutline* outline, std::uint8_t opened) {
    if (!outline) return FT_Stroker_ParseOutline(static_cast<FT_Stroker>(stroker), nullptr, opened);
    if (stroker) CheckOutlineArrays(*outline, __func__);
    std::vector<FT_Vector> converted;
    FT_Vector* points = nullptr;
    if constexpr (SharedPoints) {
        if (stroker && !StrokedPointsFitInt32(*outline)) NotImplemented_nid_no_patch(__func__);
        points = reinterpret_cast<FT_Vector*>(outline->points);
    } else if (stroker) {
        converted = NativePoints(*outline, __func__);
        points = converted.data();
    }
    FT_Outline native = NativeOutline(*outline, points);
    return FT_Stroker_ParseOutline(static_cast<FT_Stroker>(stroker), &native, opened);
}

void APS5_VABI FT_Stroker_ExportBorder_nid_postfix(void* stroker, std::int32_t border, GuestOutline* outline) {
    ExportBorder(stroker, border, outline, __func__);
}

void APS5_VABI FT_Stroker_Export_nid_postfix(void* stroker, GuestOutline* outline) {
    ExportBorder(stroker, FT_STROKER_BORDER_LEFT, outline, __func__);
    ExportBorder(stroker, FT_STROKER_BORDER_RIGHT, outline, __func__);
}

}
