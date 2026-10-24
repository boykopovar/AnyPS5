#include "prx/libSceFreeType/include/GuestObjects.hpp"
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
    std::vector<FT_Vector> points;
    FT_Outline outline{};

    NativeOutline(const GuestOutline& guest, const char* function) {
        points.reserve(guest.n_points);
        for (std::size_t index = 0; index < guest.n_points; ++index) {
            points.push_back({HostCoordinate(guest.points[index].x, function), HostCoordinate(guest.points[index].y, function)});
        }
        outline = {guest.n_contours, guest.n_points, points.data(), guest.tags, guest.contours, guest.flags};
    }

    void Publish(GuestOutline& guest) const {
        for (std::size_t index = 0; index < points.size(); ++index) guest.points[index] = {points[index].x, points[index].y};
    }
};

}

extern "C" {

int APS5_VABI FT_Outline_Embolden_nid_postfix(GuestOutline* outline, std::int64_t strength) {
    if (!outline) return FT_Outline_Embolden(nullptr, 0);
    NativeOutline native(*outline, __func__);
    const FT_Error error = FT_Outline_Embolden(&native.outline, HostCoordinate(strength, __func__));
    native.Publish(*outline);
    return error;
}

int APS5_VABI FT_Outline_EmboldenXY_nid_postfix(GuestOutline* outline, std::int64_t xStrength, std::int64_t yStrength) {
    if (!outline) return FT_Outline_EmboldenXY(nullptr, 0, 0);
    NativeOutline native(*outline, __func__);
    const FT_Error error = FT_Outline_EmboldenXY(&native.outline, HostCoordinate(xStrength, __func__), HostCoordinate(yStrength, __func__));
    native.Publish(*outline);
    return error;
}

}
