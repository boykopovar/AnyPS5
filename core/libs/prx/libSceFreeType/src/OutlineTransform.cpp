#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace FreeType;

namespace {

FT_Pos HostCoordinate(std::int64_t value, const char* function) {
    if (value < std::numeric_limits<FT_Pos>::min() || value > std::numeric_limits<FT_Pos>::max()) {
        throw std::runtime_error(std::string(function) + ": a value outside the host FT_Pos range is not implemented");
    }
    return static_cast<FT_Pos>(value);
}

struct NativeOutline {
    GuestOutline guest{};
    std::vector<FT_Vector> points;
    std::vector<unsigned char> tags;
    std::vector<std::uint16_t> contours;
    FT_Outline outline{};

    bool Read(const GuestOutline* source, const char* function) {
        if (!GuestCopyIn_nid_no_patch(&guest, source, sizeof(guest))) return false;
        std::vector<GuestVector> original(guest.n_points);
        tags.resize(guest.tags ? guest.n_points : 0);
        contours.resize(guest.n_contours);
        if (!GuestCopyIn_nid_no_patch(original.data(), guest.points, original.size() * sizeof(GuestVector)) ||
            !GuestWritable_nid_no_patch(guest.points, original.size() * sizeof(GuestVector)) ||
            !GuestCopyIn_nid_no_patch(tags.data(), guest.tags, tags.size()) ||
            !GuestCopyIn_nid_no_patch(contours.data(), guest.contours, contours.size() * sizeof(std::uint16_t))) return false;
        points.reserve(original.size());
        for (const GuestVector& point : original) points.push_back({HostCoordinate(point.x, function), HostCoordinate(point.y, function)});
        outline = {guest.n_contours, guest.n_points, points.data(), guest.tags ? tags.data() : nullptr, contours.data(), guest.flags};
        return true;
    }

    std::vector<GuestVector> Points() const {
        std::vector<GuestVector> result;
        result.reserve(points.size());
        for (const FT_Vector& point : points) result.push_back({point.x, point.y});
        return result;
    }

    bool Publish(const std::vector<GuestVector>& result) const {
        return GuestCopyOut_nid_no_patch(guest.points, result.data(), result.size() * sizeof(GuestVector));
    }
};

}

extern "C" {

int APS5_VABI FT_Outline_Embolden_nid_postfix(GuestOutline* outline, std::int64_t strength) {
    if (!outline) return FT_Outline_Embolden(nullptr, 0);
    if (strength / 2 == 0) return FT_Err_Ok;
    NativeOutline native;
    if (!native.Read(outline, __func__)) return FT_Outline_Embolden(nullptr, 0);
    const FT_Error error = FT_Outline_Embolden(&native.outline, HostCoordinate(strength, __func__));
    if (!native.Publish(native.Points())) return FT_Outline_Embolden(nullptr, 0);
    return error;
}

int APS5_VABI FT_Outline_EmboldenXY_nid_postfix(GuestOutline* outline, std::int64_t xStrength, std::int64_t yStrength) {
    if (!outline) return FT_Outline_EmboldenXY(nullptr, 0, 0);
    if (xStrength / 2 == 0 && yStrength / 2 == 0) return FT_Err_Ok;
    NativeOutline native;
    if (!native.Read(outline, __func__)) return FT_Outline_EmboldenXY(nullptr, 0, 0);
    const FT_Error error = FT_Outline_EmboldenXY(&native.outline, HostCoordinate(xStrength, __func__), HostCoordinate(yStrength, __func__));
    if (!native.Publish(native.Points())) return FT_Outline_EmboldenXY(nullptr, 0, 0);
    return error;
}

void APS5_VABI FT_Outline_Transform_nid_postfix(const GuestOutline* outline, const GuestMatrix* matrix) {
    if (!outline || !matrix) return;
    NativeOutline native;
    GuestMatrix transform{};
    if (!native.Read(outline, __func__) || !GuestCopyIn_nid_no_patch(&transform, matrix, sizeof(transform))) return;
    const FT_Matrix host{HostCoordinate(transform.xx, __func__), HostCoordinate(transform.xy, __func__), HostCoordinate(transform.yx, __func__),
                         HostCoordinate(transform.yy, __func__)};
    std::vector<GuestVector> result;
    if constexpr (sizeof(FT_Pos) < sizeof(std::int64_t)) {
        const GuestMatrix hostTransform{host.xx, host.xy, host.yx, host.yy};
        for (const FT_Vector& point : native.points) {
            GuestVector value{point.x, point.y};
            FT_Vector_Transform_nid_postfix(&value, &hostTransform);
            result.push_back(value);
        }
    } else {
        FT_Outline_Transform(&native.outline, &host);
        result = native.Points();
    }
    native.Publish(result);
}

}
