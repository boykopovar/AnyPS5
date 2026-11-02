#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

static constexpr float DEFAULT_SOUND_SPEED = 343.0f;

namespace {

using Vector = std::array<double, 3>;

Vector ReadVector(const Ngs2GeomVector& value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) APS5_INVALID_ARG_EX;
    return {value.x, value.y, value.z};
}

double Dot(const Vector& left, const Vector& right) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

Vector Cross(const Vector& left, const Vector& right) {
    return {left[1] * right[2] - left[2] * right[1], left[2] * right[0] - left[0] * right[2], left[0] * right[1] - left[1] * right[0]};
}

Vector Unit(Vector value) {
    const double length = std::hypot(value[0], value[1], value[2]);
    if (!(length > 0.0)) APS5_INVALID_ARG_EX;
    for (auto& component : value) component /= length;
    return value;
}

Vector PerpendicularHint(const Vector& front) {
    return std::abs(front[1]) < 0.9 ? Vector{0.0, 1.0, 0.0} : Vector{1.0, 0.0, 0.0};
}

}

int Ngs2GeomCalcListener(const Ngs2GeomListenerParam* param, Ngs2GeomListenerWork* outWork, std::uint32_t flags) {
    if (param == nullptr) return SCE_NGS2_ERROR_INVALID_GEOM_LISTENER_ADDRESS;
    if (outWork == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (flags > 1) return SCE_NGS2_ERROR_INVALID_GEOM_FLAG;
    if (!std::isfinite(param->sound_speed) || param->sound_speed <= 0.0f) APS5_INVALID_ARG_EX;
    const Vector position = ReadVector(param->position);
    ReadVector(param->velocity);
    const Vector front = Unit(ReadVector(param->orient_front));
    Vector up = Unit(ReadVector(param->orient_up));
    Vector right = Cross(up, front);
    if (!(std::hypot(right[0], right[1], right[2]) > 0.0)) {
        up = Unit(PerpendicularHint(front));
        right = Cross(up, front);
    }
    right = Unit(right);
    up = Cross(front, right);
    if (flags == 1) for (auto& component : right) component = -component;
    const std::array<Vector, 3> axes{right, up, front};
    Ngs2GeomListenerWork work{};
    for (std::size_t row = 0; row < axes.size(); row++) {
        for (std::size_t column = 0; column < 3; column++) work.matrix[row][column] = static_cast<float>(axes[row][column]);
        const double translation = -Dot(axes[row], position);
        if (std::abs(translation) > std::numeric_limits<float>::max()) APS5_INVALID_ARG_EX;
        work.matrix[row][3] = static_cast<float>(translation);
    }
    work.matrix[3][3] = 1.0f;
    work.velocity = param->velocity;
    work.sound_speed = param->sound_speed;
    work.coordinate = flags;
    *outWork = work;
    return SCE_NGS2_OK;
}

int Ngs2GeomResetListenerParam(Ngs2GeomListenerParam* out_listener_param) {
    if (out_listener_param == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *out_listener_param = {};
    out_listener_param->orient_front.z = 1.0f;
    out_listener_param->orient_up.y = 1.0f;
    out_listener_param->sound_speed = DEFAULT_SOUND_SPEED;
    return SCE_NGS2_OK;
}

int Ngs2GeomResetSourceParam(Ngs2GeomSourceParam* out_source_param) {
    if (out_source_param == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *out_source_param = {};
    out_source_param->direction.z = 1.0f;
    out_source_param->cone.inner_level = 1.0f;
    out_source_param->cone.inner_angle = 360.0f;
    out_source_param->cone.outer_level = 1.0f;
    out_source_param->cone.outer_angle = 360.0f;
    out_source_param->rolloff.max_distance = 1000000.0f;
    out_source_param->rolloff.rolloff_factor = 1.0f;
    out_source_param->rolloff.reference_distance = 1.0f;
    out_source_param->doppler_factor = 1.0f;
    out_source_param->fbw_level = 1.0f;
    out_source_param->lfe_level = 1.0f;
    out_source_param->max_level = 1.0f;
    out_source_param->num_speakers = 2;
    out_source_param->matrix_format = 2;
    return SCE_NGS2_OK;
}

int Ngs2GeomApply(const Ngs2GeomListenerWork* listener, const Ngs2GeomSourceParam* source, Ngs2GeomAttribute* out_attrib, std::uint32_t flags) {
    if (listener == nullptr || source == nullptr) APS5_INVALID_ARG_EX;
    if (out_attrib == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    (void)flags;
    std::memset(out_attrib, 0, sizeof(*out_attrib));
    out_attrib->pitch_ratio = 1.0f;
    out_attrib->a3d_attrib.position = source->position;
    out_attrib->a3d_attrib.volume = std::max(source->min_level, source->max_level);
    const std::uint32_t channels = std::min(source->matrix_format == 0u ? 2u : source->matrix_format, 8u);
    const float level = source->max_level > 0.0f ? source->max_level : 1.0f;
    for (std::uint32_t channel = 0; channel < channels; channel++) {
        out_attrib->level[static_cast<std::size_t>(channel) * 8 + channel] = level;
    }
    return SCE_NGS2_OK;
}
