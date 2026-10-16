#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stdexcept>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

static constexpr float DEFAULT_SOUND_SPEED = 343.0f;
static constexpr std::uint32_t GEOM_FLAG_SOURCE_ANGLE = 1;
static constexpr std::uint32_t GEOM_FLAG_SOURCE_DOPPLER = 2;
static constexpr std::uint32_t GEOM_FLAG_VOLUME_MATRIX = 4;
static constexpr std::uint32_t GEOM_FLAG_A3D_ATTRIBUTE = 8;
static constexpr std::uint32_t GEOM_FLAG_A3D_AMBISONICS = 16;
static constexpr std::uint32_t GEOM_DEFAULT_FLAGS = GEOM_FLAG_SOURCE_ANGLE | GEOM_FLAG_SOURCE_DOPPLER |
    GEOM_FLAG_VOLUME_MATRIX | GEOM_FLAG_A3D_ATTRIBUTE;
static constexpr std::uint32_t GEOM_SUPPORTED_FLAGS = GEOM_FLAG_SOURCE_ANGLE | GEOM_FLAG_SOURCE_DOPPLER |
    GEOM_FLAG_VOLUME_MATRIX | GEOM_FLAG_A3D_ATTRIBUTE | GEOM_FLAG_A3D_AMBISONICS;

static float Dot(const Ngs2GeomVector& a, const Ngs2GeomVector& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float Length(const Ngs2GeomVector& value) {
    return std::sqrt(Dot(value, value));
}

static Ngs2GeomVector Scale(const Ngs2GeomVector& value, float factor) {
    return {value.x * factor, value.y * factor, value.z * factor};
}

static Ngs2GeomVector TransformPoint(const Ngs2GeomListenerWork& listener, const Ngs2GeomVector& point) {
    return {
        listener.matrix[0][0] * point.x + listener.matrix[0][1] * point.y + listener.matrix[0][2] * point.z + listener.matrix[0][3],
        listener.matrix[1][0] * point.x + listener.matrix[1][1] * point.y + listener.matrix[1][2] * point.z + listener.matrix[1][3],
        listener.matrix[2][0] * point.x + listener.matrix[2][1] * point.y + listener.matrix[2][2] * point.z + listener.matrix[2][3],
    };
}

static Ngs2GeomVector Rotate(const Ngs2GeomListenerWork& listener, const Ngs2GeomVector& vector) {
    return {
        listener.matrix[0][0] * vector.x + listener.matrix[0][1] * vector.y + listener.matrix[0][2] * vector.z,
        listener.matrix[1][0] * vector.x + listener.matrix[1][1] * vector.y + listener.matrix[1][2] * vector.z,
        listener.matrix[2][0] * vector.x + listener.matrix[2][1] * vector.y + listener.matrix[2][2] * vector.z,
    };
}

static bool Finite(const Ngs2GeomVector& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

static float DistanceGain(const Ngs2GeomSourceParam& source, float distance) {
    const float reference = source.rolloff.reference_distance > 0.0f ? source.rolloff.reference_distance : 1.0f;
    const float maximum = std::max(source.rolloff.max_distance, reference);
    const float clampedDistance = std::clamp(distance, reference, maximum);
    const float factor = std::max(source.rolloff.rolloff_factor, 0.0f);
    const std::uint32_t model = source.rolloff.model >= 3 ? source.rolloff.model - 3 : source.rolloff.model;
    float gain = 1.0f;
    switch (model) {
    case 0:
        gain = reference / (reference + factor * (clampedDistance - reference));
        break;
    case 1:
        gain = maximum > reference ? 1.0f - factor * (clampedDistance - reference) / (maximum - reference) : 1.0f;
        break;
    case 2:
        gain = std::pow(clampedDistance / reference, -factor);
        break;
    default:
        throw std::runtime_error("NGS2: unknown geometry rolloff model");
    }
    return std::clamp(gain, 0.0f, 1.0f);
}

static float ConeGain(const Ngs2GeomListenerWork& listener, const Ngs2GeomSourceParam& source,
    const Ngs2GeomVector& local, float distance) {
    const auto direction = Rotate(listener, source.direction);
    const float directionLength = Length(direction);
    if (!(directionLength > 1e-6f) || !std::isfinite(directionLength) || distance <= 1e-6f) return source.cone.inner_level;
    const float cosine = std::clamp(Dot(Scale(direction, 1.0f / directionLength), Scale(local, -1.0f / distance)), -1.0f, 1.0f);
    const float aperture = 2.0f * 180.0f / std::numbers::pi_v<float> * std::acos(cosine);
    const float inner = std::clamp(source.cone.inner_angle, 0.0f, 360.0f);
    const float outer = std::clamp(std::max(source.cone.outer_angle, inner), 0.0f, 360.0f);
    if (aperture <= inner) return source.cone.inner_level;
    if (aperture >= outer || outer <= inner) return source.cone.outer_level;
    const float position = (aperture - inner) / (outer - inner);
    return source.cone.inner_level + (source.cone.outer_level - source.cone.inner_level) * position;
}

static float DopplerRatio(const Ngs2GeomListenerWork& listener, const Ngs2GeomSourceParam& source,
    const Ngs2GeomVector& local, float distance) {
    if (source.doppler_factor <= 0.0f || distance <= 1e-6f) return 1.0f;
    const float limit = listener.sound_speed / source.doppler_factor;
    const auto axis = Scale(local, -1.0f / distance);
    const float listenerSpeed = std::min(Dot(Rotate(listener, listener.velocity), axis), limit * 0.999f);
    const float sourceSpeed = std::min(Dot(Rotate(listener, source.velocity), axis), limit * 0.999f);
    const float ratio = (listener.sound_speed - source.doppler_factor * listenerSpeed) /
        (listener.sound_speed - source.doppler_factor * sourceSpeed);
    return std::isfinite(ratio) ? std::clamp(ratio, 1.0f / 16.0f, 16.0f) : 1.0f;
}

static void ValidateGeomApply(const Ngs2GeomListenerWork& listener, const Ngs2GeomSourceParam& source, std::uint32_t flags) {
    if ((flags & ~GEOM_SUPPORTED_FLAGS) != 0) APS5_INVALID_ARG_EX;
    for (const auto& row : listener.matrix) {
        for (float value : row) if (!std::isfinite(value)) APS5_INVALID_ARG_EX;
    }
    if (!Finite(listener.velocity) || !std::isfinite(listener.sound_speed) || listener.sound_speed <= 0.0f) APS5_INVALID_ARG_EX;
    if (!Finite(source.position) || !Finite(source.velocity) || !Finite(source.direction)) APS5_INVALID_ARG_EX;
    const float sourceValues[] = {
        source.cone.inner_level, source.cone.inner_angle, source.cone.outer_level, source.cone.outer_angle,
        source.rolloff.max_distance, source.rolloff.rolloff_factor, source.rolloff.reference_distance,
        source.doppler_factor, source.fbw_level, source.lfe_level, source.max_level, source.min_level, source.radius,
    };
    for (float value : sourceValues) if (!std::isfinite(value)) APS5_INVALID_ARG_EX;
    if (source.num_speakers != 2 && source.num_speakers != 4 && source.num_speakers != 5 && source.num_speakers != 7) APS5_INVALID_ARG_EX;
    if (source.matrix_format != 1 && source.matrix_format != 2 && source.matrix_format != 6 && source.matrix_format != 8) APS5_INVALID_ARG_EX;
}

static std::uint32_t GeometryOutputChannels(std::uint32_t numSpeakers) {
    switch (numSpeakers) {
    case 2: return 2;
    case 4:
    case 5: return 6;
    case 7: return 8;
    default: APS5_INVALID_ARG_EX;
    }
}

namespace {

using Vector = std::array<double, 3>;

Vector ReadVector(const Ngs2GeomVector& value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) APS5_INVALID_ARG_EX;
    return {value.x, value.y, value.z};
}

double Dot(const Vector& left, const Vector& right) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

Vector Unit(Vector value) {
    const double length = std::hypot(value[0], value[1], value[2]);
    if (!(length > 0.0)) APS5_INVALID_ARG_EX;
    for (auto& component : value) component /= length;
    return value;
}

}

int Ngs2GeomCalcListener(const Ngs2GeomListenerParam* param, Ngs2GeomListenerWork* outWork, std::uint32_t flags) {
    if (param == nullptr) APS5_INVALID_ARG_EX;
    if (outWork == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (flags > 1) throw std::runtime_error("NGS2: unknown listener coordinate flags");
    if (!std::isfinite(param->sound_speed) || param->sound_speed <= 0.0f) APS5_INVALID_ARG_EX;
    const Vector position = ReadVector(param->position);
    ReadVector(param->velocity);
    const Vector front = Unit(ReadVector(param->orient_front));
    Vector up = Unit(ReadVector(param->orient_up));
    Vector right = Unit({up[1] * front[2] - up[2] * front[1], up[2] * front[0] - up[0] * front[2], up[0] * front[1] - up[1] * front[0]});
    up = {front[1] * right[2] - front[2] * right[1], front[2] * right[0] - front[0] * right[2], front[0] * right[1] - front[1] * right[0]};
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

int Ngs2GeomApply(const Ngs2GeomListenerWork* listener, const Ngs2GeomSourceParam* source,
    Ngs2GeomAttribute* out_attrib, std::uint32_t flags) {
    if (listener == nullptr || source == nullptr) APS5_INVALID_ARG_EX;
    if (out_attrib == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    ValidateGeomApply(*listener, *source, flags);
    if (flags == 0) flags = GEOM_DEFAULT_FLAGS;

    Ngs2GeomAttribute attribute{};
    attribute.pitch_ratio = 1.0f;
    const auto local = TransformPoint(*listener, source->position);
    const float distance = Length(local);
    const float level = std::clamp(
        DistanceGain(*source, distance) * ConeGain(*listener, *source, local, distance),
        std::min(source->min_level, source->max_level), source->max_level);
    if ((flags & GEOM_FLAG_SOURCE_DOPPLER) != 0) attribute.pitch_ratio = DopplerRatio(*listener, *source, local, distance);
    if ((flags & (GEOM_FLAG_SOURCE_ANGLE | GEOM_FLAG_A3D_ATTRIBUTE)) != 0) {
        attribute.a3d_attrib.position = local;
        attribute.a3d_attrib.volume = level;
    }
    if ((flags & (GEOM_FLAG_SOURCE_ANGLE | GEOM_FLAG_VOLUME_MATRIX)) != 0) {
        const float angle = distance > 1e-6f ? std::atan2(local.x, local.z) * 180.0f / std::numbers::pi_v<float> : 0.0f;
        const float panDistance = distance > 1e-6f ? std::clamp(local.z / distance, -1.0f, 1.0f) : 1.0f;
        Ngs2PanWork pan{};
        Ngs2PanParam param{angle, panDistance, source->fbw_level, source->lfe_level};
        const std::uint32_t outputChannels = GeometryOutputChannels(source->num_speakers);
        float panLevels[NGS2_MAX_CHANNELS] = {};
        if (Ngs2PanInit(&pan, nullptr, 360.0f, source->num_speakers) != SCE_NGS2_OK ||
            Ngs2PanGetVolumeMatrix(&pan, &param, 1, outputChannels, panLevels) != SCE_NGS2_OK) {
            throw std::runtime_error("NGS2: geometry pan calculation failed");
        }
        const float radiusWeight = distance > 1e-6f ? 1.0f - std::clamp(source->radius / distance, 0.0f, 1.0f) : 0.0f;
        const bool hasLfe = outputChannels == 6 || outputChannels == 8;
        const std::uint32_t fullBandChannels = outputChannels - (hasLfe ? 1 : 0);
        const float evenLevel = source->fbw_level / std::sqrt(static_cast<float>(fullBandChannels));
        for (std::uint32_t output = 0; output < outputChannels; output++) {
            if (hasLfe && output == 3) continue;
            panLevels[output] = radiusWeight * panLevels[output] + (1.0f - radiusWeight) * evenLevel;
        }
        for (std::uint32_t input = 0; input < source->matrix_format; input++) {
            for (std::uint32_t output = 0; output < outputChannels; output++) {
                attribute.level[input * NGS2_MAX_CHANNELS + output] = panLevels[output] * level;
            }
        }
    }
    *out_attrib = attribute;
    return SCE_NGS2_OK;
}
