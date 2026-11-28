#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_STROKER_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

using namespace FreeType;

namespace {

constexpr bool SharedPoints = sizeof(FT_Pos) == sizeof(std::int64_t);

enum class SizeState { Uncounted, Current, Stale };

struct BorderSize {
    FT_UInt points = 0;
    FT_UInt contours = 0;
    SizeState state = SizeState::Uncounted;
};

struct GuestStroker {
    FT_Stroker host;
    BorderSize borders[2]{};
};

GuestStroker* StrokerState(void* stroker) {
    if constexpr (SharedPoints) return nullptr;
    return static_cast<GuestStroker*>(stroker);
}

FT_Stroker NativeStroker(void* stroker) {
    if constexpr (SharedPoints) return static_cast<FT_Stroker>(stroker);
    return stroker ? StrokerState(stroker)->host : nullptr;
}

void ReleaseStroker(void* stroker) {
    FT_Stroker_Done(NativeStroker(stroker));
    delete StrokerState(stroker);
}

FT_Stroker ChangeStroke(void* stroker) {
    if (auto* state = StrokerState(stroker)) {
        for (auto& border : state->borders) {
            if (border.state == SizeState::Current) border.state = SizeState::Stale;
        }
    }
    return NativeStroker(stroker);
}

void ResetStroke(void* stroker) {
    if (auto* state = StrokerState(stroker)) {
        for (auto& border : state->borders) border = {};
    }
}

int CountBorder(GuestStroker& stroker, std::int32_t border, FT_UInt& points, FT_UInt& contours) {
    if (border < 0 || border > 1) return FT_Err_Invalid_Argument;
    const int error = FT_Stroker_GetBorderCounts(stroker.host, static_cast<FT_StrokerBorder>(border), &points, &contours);
    if (!error) stroker.borders[border] = {points, contours, SizeState::Current};
    return error;
}

FT_Library ReadGuestLibrary(GuestLibrary* library, const char* function) {
    if (!library) return nullptr;
    FT_Library host = nullptr;
    const bool read = reinterpret_cast<std::uintptr_t>(library) % alignof(GuestLibrary) == 0 && GuestCopyIn_nid_no_patch(&host, &library->host, sizeof(host));
    if (!read) throw std::invalid_argument(std::string(function) + ": unreadable library handle");
    return host;
}

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

int WriteCounts(std::uint32_t* points, std::uint32_t* contours, FT_UInt nativePoints, FT_UInt nativeContours, int error) {
    GuestArena::HostWrite pointsWrite(points, points ? sizeof(*points) : 0);
    GuestArena::HostWrite contoursWrite(contours, contours ? sizeof(*contours) : 0);
    if ((points && (!pointsWrite.Open() || !GuestWritable_nid_no_patch(points, sizeof(*points)))) ||
        (contours && (!contoursWrite.Open() || !GuestWritable_nid_no_patch(contours, sizeof(*contours))))) return error != 0 ? error : FT_Err_Invalid_Argument;
    if (points && !GuestCopyOut_nid_no_patch(points, &nativePoints, sizeof(nativePoints))) return error != 0 ? error : FT_Err_Invalid_Argument;
    if (contours && !GuestCopyOut_nid_no_patch(contours, &nativeContours, sizeof(nativeContours))) return error != 0 ? error : FT_Err_Invalid_Argument;
    return error;
}

FT_Outline NativeOutline(const GuestOutline& outline, FT_Vector* points) {
    return {outline.n_contours, outline.n_points, points, outline.tags, outline.contours, outline.flags};
}

bool FitsInt32(std::int64_t value) {
    return value >= std::numeric_limits<std::int32_t>::min() && value <= std::numeric_limits<std::int32_t>::max();
}

struct OutlineArrays {
    std::vector<GuestVector> points;
    std::vector<unsigned char> tags;
    std::vector<std::uint16_t> contours;
};

OutlineArrays ReadOutlineArrays(const GuestOutline& outline, const char* function) {
    OutlineArrays arrays;
    if (outline.n_contours == 0) return arrays;
    arrays.points.resize(outline.n_points);
    arrays.tags.resize(outline.n_points);
    arrays.contours.resize(outline.n_contours);
    if (!outline.points || !outline.tags || !outline.contours ||
        !GuestCopyIn_nid_no_patch(arrays.points.data(), outline.points, arrays.points.size() * sizeof(GuestVector)) ||
        !GuestCopyIn_nid_no_patch(arrays.tags.data(), outline.tags, arrays.tags.size()) ||
        !GuestCopyIn_nid_no_patch(arrays.contours.data(), outline.contours, arrays.contours.size() * sizeof(std::uint16_t))) {
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    for (const std::uint16_t last : arrays.contours) {
        if (last >= outline.n_points) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    return arrays;
}

bool StrokedPointsFitInt32(const OutlineArrays& arrays) {
    std::int32_t last = -1;
    for (const std::uint16_t end : arrays.contours) {
        const std::int32_t first = last + 1;
        last = end;
        if (last <= first) continue;
        for (std::int32_t index = first; index <= last; ++index) {
            if (!FitsInt32(arrays.points[index].x) || !FitsInt32(arrays.points[index].y)) return false;
        }
    }
    return true;
}

std::vector<FT_Vector> NativePoints(const OutlineArrays& arrays, const char* function) {
    std::vector<FT_Vector> points;
    points.reserve(arrays.points.size());
    for (const GuestVector& point : arrays.points) points.push_back({HostCoordinate(point.x, function), HostCoordinate(point.y, function)});
    return points;
}

void ExportWide(const GuestStroker& stroker, unsigned borders, GuestOutline* outline, const char* function) {
    std::size_t points = 0;
    std::size_t contours = 0;
    for (unsigned border = 0; border < 2; ++border) {
        if (!(borders & (1u << border))) continue;
        const auto& count = stroker.borders[border];
        if (count.state == SizeState::Stale) {
            throw std::runtime_error(std::string(function) + ": stroke changed after counting");
        }
        points += count.points;
        contours += count.contours;
    }
    if (!points) return;

    GuestOutline header{};
    if (!GuestCopyIn_nid_no_patch(&header, outline, sizeof(header)) || !header.points || !header.tags || !header.contours) {
        throw std::runtime_error(std::string(function) + ": inaccessible outline storage");
    }
    constexpr auto maxCount = std::numeric_limits<std::uint16_t>::max();
    if (points + header.n_points > maxCount || contours + header.n_contours > maxCount) {
        throw std::runtime_error(std::string(function) + ": outline counts exceed 16 bits");
    }

    auto* pointTail = header.points + header.n_points;
    auto* tagTail = header.tags + header.n_points;
    auto* contourTail = header.contours + header.n_contours;
    const auto pointBytes = points * sizeof(*pointTail);
    const auto contourBytes = contours * sizeof(*contourTail);
    constexpr auto countBytes = sizeof(header.n_contours) + sizeof(header.n_points);
    static_assert(offsetof(GuestOutline, n_points) == sizeof(header.n_contours));
    GuestArena::HostWrite countWrite(outline, countBytes);
    GuestArena::HostWrite pointWrite(pointTail, pointBytes);
    GuestArena::HostWrite tagWrite(tagTail, points);
    GuestArena::HostWrite contourWrite(contourTail, contourBytes);
    if (!countWrite.Open() || !GuestWritable_nid_no_patch(outline, countBytes) ||
        !pointWrite.Open() || !GuestWritable_nid_no_patch(pointTail, pointBytes) ||
        !tagWrite.Open() || !GuestWritable_nid_no_patch(tagTail, points) ||
        !contourWrite.Open() || !GuestWritable_nid_no_patch(contourTail, contourBytes)) {
        throw std::runtime_error(std::string(function) + ": unwritable outline storage");
    }

    const auto nativePoints = std::make_unique_for_overwrite<FT_Vector[]>(points);
    FT_Outline native{0, 0, nativePoints.get(), tagTail, contourTail, header.flags};
    if (borders == 3) FT_Stroker_Export(stroker.host, &native);
    else FT_Stroker_ExportBorder(stroker.host, borders == 1 ? FT_STROKER_BORDER_LEFT : FT_STROKER_BORDER_RIGHT, &native);
    for (std::size_t index = 0; index < points; ++index) pointTail[index] = {nativePoints[index].x, nativePoints[index].y};
    for (std::size_t index = 0; index < contours; ++index) contourTail[index] += header.n_points;
    outline->n_points = static_cast<std::uint16_t>(header.n_points + points);
    outline->n_contours = static_cast<std::uint16_t>(header.n_contours + contours);
}

void ExportStroke(void* stroker, unsigned borders, GuestOutline* outline, const char* function) {
    if (!stroker || !outline) return;
    if constexpr (SharedPoints) {
        FT_Outline native = NativeOutline(*outline, reinterpret_cast<FT_Vector*>(outline->points));
        if (borders == 3) FT_Stroker_Export(NativeStroker(stroker), &native);
        else FT_Stroker_ExportBorder(NativeStroker(stroker), borders == 1 ? FT_STROKER_BORDER_LEFT : FT_STROKER_BORDER_RIGHT, &native);
        outline->n_contours = native.n_contours;
        outline->n_points = native.n_points;
    } else {
        ExportWide(*StrokerState(stroker), borders, outline, function);
    }
}

}

extern "C" void* FreeType::FreeTypeHostStroker_nid_no_patch(void* stroker) {
    return ChangeStroke(stroker);
}

extern "C" {

int APS5_VABI FT_Stroker_New_nid_postfix(GuestLibrary* library, void** stroker) {
    GuestArena::HostWrite strokerWrite(stroker, stroker ? sizeof(*stroker) : 0);
    if (stroker && (!strokerWrite.Open() || !GuestWritable_nid_no_patch(stroker, sizeof(*stroker)))) return FT_Err_Invalid_Argument;
    const FT_Library host = ReadGuestLibrary(library, __func__);
    FT_Stroker native = nullptr;
    int error = FT_Stroker_New(host, stroker ? &native : nullptr);
    if (!stroker || !host) return error;
    void* guest = native;
    if constexpr (!SharedPoints) {
        if (native) {
            guest = new (std::nothrow) GuestStroker{native};
            if (!guest) {
                FT_Stroker_Done(native);
                error = FT_Err_Out_Of_Memory;
            }
        }
    }
    if (!GuestCopyOut_nid_no_patch(stroker, &guest, sizeof(guest))) {
        ReleaseStroker(guest);
        return FT_Err_Invalid_Argument;
    }
    return error;
}

void APS5_VABI FT_Stroker_Done_nid_postfix(void* stroker) {
    ReleaseStroker(stroker);
}

int APS5_VABI FT_Stroker_GetCounts_nid_postfix(void* stroker, std::uint32_t* points, std::uint32_t* contours) {
    FT_UInt nativePoints = 0;
    FT_UInt nativeContours = 0;
    if (auto* state = StrokerState(stroker)) {
        FT_UInt rightPoints = 0;
        FT_UInt rightContours = 0;
        int error = CountBorder(*state, 0, nativePoints, nativeContours);
        if (!error) error = CountBorder(*state, 1, rightPoints, rightContours);
        return WriteCounts(points, contours, error ? 0 : nativePoints + rightPoints,
                           error ? 0 : nativeContours + rightContours, error);
    }
    const FT_Error error = FT_Stroker_GetCounts(NativeStroker(stroker), points ? &nativePoints : nullptr, contours ? &nativeContours : nullptr);
    return WriteCounts(points, contours, nativePoints, nativeContours, error);
}

int APS5_VABI FT_Stroker_BeginSubPath_nid_postfix(void* stroker, const GuestVector* to, std::uint8_t open) {
    NativeVector vector(to, __func__);
    return FT_Stroker_BeginSubPath(NativeStroker(stroker), vector.pointer, open);
}

int APS5_VABI FT_Stroker_LineTo_nid_postfix(void* stroker, const GuestVector* to) {
    NativeVector vector(to, __func__);
    return FT_Stroker_LineTo(ChangeStroke(stroker), vector.pointer);
}

int APS5_VABI FT_Stroker_EndSubPath_nid_postfix(void* stroker) {
    return FT_Stroker_EndSubPath(ChangeStroke(stroker));
}

void APS5_VABI FT_Stroker_Set_nid_postfix(void* stroker, std::int64_t radius, std::int32_t lineCap, std::int32_t lineJoin, std::int64_t miterLimit) {
    FT_Stroker_Set(NativeStroker(stroker), HostCoordinate(radius, __func__), static_cast<FT_Stroker_LineCap>(lineCap),
                   static_cast<FT_Stroker_LineJoin>(lineJoin), HostCoordinate(miterLimit, __func__));
    ResetStroke(stroker);
}

void APS5_VABI FT_Stroker_Rewind_nid_postfix(void* stroker) {
    FT_Stroker_Rewind(NativeStroker(stroker));
    ResetStroke(stroker);
}

int APS5_VABI FT_Stroker_GetBorderCounts_nid_postfix(void* stroker, std::int32_t border, std::uint32_t* points, std::uint32_t* contours) {
    FT_UInt nativePoints = 0;
    FT_UInt nativeContours = 0;
    auto* state = StrokerState(stroker);
    const FT_Error error = state ? CountBorder(*state, border, nativePoints, nativeContours) :
        FT_Stroker_GetBorderCounts(NativeStroker(stroker), static_cast<FT_StrokerBorder>(border),
                                  points ? &nativePoints : nullptr, contours ? &nativeContours : nullptr);
    return WriteCounts(points, contours, nativePoints, nativeContours, error);
}

int APS5_VABI FT_Stroker_ConicTo_nid_postfix(void* stroker, const GuestVector* control, const GuestVector* to) {
    NativeVector nativeControl(control, __func__);
    NativeVector nativeTo(to, __func__);
    return FT_Stroker_ConicTo(ChangeStroke(stroker), nativeControl.pointer, nativeTo.pointer);
}

int APS5_VABI FT_Stroker_CubicTo_nid_postfix(void* stroker, const GuestVector* control1, const GuestVector* control2, const GuestVector* to) {
    NativeVector first(control1, __func__);
    NativeVector second(control2, __func__);
    NativeVector end(to, __func__);
    return FT_Stroker_CubicTo(ChangeStroke(stroker), first.pointer, second.pointer, end.pointer);
}

int APS5_VABI FT_Stroker_ParseOutline_nid_postfix(void* stroker, const GuestOutline* outline, std::uint8_t opened) {
    GuestOutline header{};
    if (!outline || !GuestCopyIn_nid_no_patch(&header, outline, sizeof(header))) return FT_Stroker_ParseOutline(NativeStroker(stroker), nullptr, opened);
    OutlineArrays arrays;
    if (stroker) arrays = ReadOutlineArrays(header, __func__);
    std::vector<FT_Vector> converted;
    FT_Vector* points = nullptr;
    if constexpr (SharedPoints) {
        if (stroker && !StrokedPointsFitInt32(arrays)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        points = reinterpret_cast<FT_Vector*>(arrays.points.data());
    } else if (stroker) {
        converted = NativePoints(arrays, __func__);
        points = converted.data();
    }
    FT_Outline native{header.n_contours, header.n_points, points, arrays.tags.data(), arrays.contours.data(), header.flags};
    ResetStroke(stroker);
    return FT_Stroker_ParseOutline(NativeStroker(stroker), &native, opened);
}

void APS5_VABI FT_Stroker_ExportBorder_nid_postfix(void* stroker, std::int32_t border, GuestOutline* outline) {
    if (border == FT_STROKER_BORDER_LEFT || border == FT_STROKER_BORDER_RIGHT) ExportStroke(stroker, 1u << border, outline, __func__);
}

void StrokerExport_nid_no_patch(void* stroker, GuestOutline* outline) {
    ExportStroke(stroker, 3, outline, "FT_Stroker_Export_nid_postfix");
}

void APS5_VABI FT_Stroker_Export_nid_postfix(void* stroker, GuestOutline* outline) {
    StrokerExport_nid_no_patch(stroker, outline);
}

}
