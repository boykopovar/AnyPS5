#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>

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

Vector Unit(Vector value) {
    const double length = std::hypot(value[0], value[1], value[2]);
    if (!(length > 0.0)) APS5_INVALID_ARG_EX;
    for (auto& component : value) component /= length;
    return value;
}

std::optional<Vector> TryUnit(Vector value) {
    const double length = std::hypot(value[0], value[1], value[2]);
    if (!(length > 1e-6) || !std::isfinite(length)) return std::nullopt;
    for (auto& component : value) component /= length;
    return value;
}

Vector TransformPoint(const Ngs2GeomListenerWork& work, const Ngs2GeomVector& point) {
    Vector result{};
    for (std::size_t row = 0; row < 3; row++) {
        result[row] = work.matrix[row][0] * point.x + work.matrix[row][1] * point.y + work.matrix[row][2] * point.z + work.matrix[row][3];
    }
    return result;
}

Vector RotateVector(const Ngs2GeomListenerWork& work, const Ngs2GeomVector& value) {
    Vector result{};
    for (std::size_t row = 0; row < 3; row++) {
        result[row] = work.matrix[row][0] * value.x + work.matrix[row][1] * value.y + work.matrix[row][2] * value.z;
    }
    return result;
}

bool FiniteVector(const Ngs2GeomVector& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void RequireFinite(const Ngs2GeomListenerWork& work, const Ngs2GeomSourceParam& source) {
    for (const auto& row : work.matrix) {
        for (const float value : row) {
            if (!std::isfinite(value)) APS5_INVALID_ARG_EX;
        }
    }
    if (!FiniteVector(work.velocity) || !std::isfinite(work.sound_speed)) APS5_INVALID_ARG_EX;
    if (!FiniteVector(source.position) || !FiniteVector(source.velocity) || !FiniteVector(source.direction)) APS5_INVALID_ARG_EX;
    if (!std::isfinite(source.cone.inner_level) || !std::isfinite(source.cone.inner_angle)
        || !std::isfinite(source.cone.outer_level) || !std::isfinite(source.cone.outer_angle)) APS5_INVALID_ARG_EX;
    if (!std::isfinite(source.rolloff.max_distance) || !std::isfinite(source.rolloff.rolloff_factor)
        || !std::isfinite(source.rolloff.reference_distance)) APS5_INVALID_ARG_EX;
    if (!std::isfinite(source.doppler_factor) || !std::isfinite(source.fbw_level) || !std::isfinite(source.lfe_level)
        || !std::isfinite(source.max_level) || !std::isfinite(source.min_level) || !std::isfinite(source.radius)) APS5_INVALID_ARG_EX;
}

constexpr std::uint32_t GEOM_MAX_CHANNELS = 8;

constexpr std::uint32_t GEOM_FLAG_SOURCE_ANGLE = 1;
constexpr std::uint32_t GEOM_FLAG_SOURCE_DOPPLER = 2;
constexpr std::uint32_t GEOM_FLAG_VOLUME_MATRIX = 4;
constexpr std::uint32_t GEOM_FLAG_A3D_ATTRIBUTE = 8;
constexpr std::uint32_t GEOM_FLAG_A3D_AMBISONICS = 16;
constexpr std::uint32_t GEOM_FLAG_DEFAULT =
    GEOM_FLAG_SOURCE_ANGLE | GEOM_FLAG_SOURCE_DOPPLER | GEOM_FLAG_VOLUME_MATRIX | GEOM_FLAG_A3D_ATTRIBUTE;
constexpr std::uint32_t GEOM_SUPPORTED_FLAGS = GEOM_FLAG_DEFAULT | GEOM_FLAG_A3D_AMBISONICS;

struct GeomSpeaker {
    double azimuth;
    bool lfe;
};

struct GeomLayout {
    std::uint32_t count;
    GeomSpeaker channels[GEOM_MAX_CHANNELS];
};

GeomLayout SpeakerLayout(std::uint32_t numSpeakers) {
    if (numSpeakers == 1) return {1, {{0.0, false}}};
    if (numSpeakers == 6 || numSpeakers == 7) {
        return {6, {{-30.0, false}, {30.0, false}, {0.0, false}, {0.0, true}, {-110.0, false}, {110.0, false}}};
    }
    if (numSpeakers == 0 || (numSpeakers >= 2 && numSpeakers <= 5)) {
        return {2, {{-90.0, false}, {90.0, false}}};
    }
    return {8, {{-30.0, false}, {30.0, false}, {0.0, false}, {0.0, true}, {-90.0, false}, {90.0, false}, {-150.0, false}, {150.0, false}}};
}

double Wrap360(double degrees) {
    const double wrapped = std::fmod(degrees, 360.0);
    return wrapped < 0.0 ? wrapped + 360.0 : wrapped;
}

void PairwiseGains(const GeomLayout& layout, double azimuth, double (&gains)[GEOM_MAX_CHANNELS]) {
    std::optional<std::uint32_t> left;
    std::optional<std::uint32_t> right;
    double leftGap = 361.0;
    double rightGap = 361.0;
    for (std::uint32_t index = 0; index < layout.count; index++) {
        if (layout.channels[index].lfe) continue;
        const double clockwise = Wrap360(layout.channels[index].azimuth - azimuth);
        const double counter = Wrap360(azimuth - layout.channels[index].azimuth);
        if (clockwise < rightGap) {
            rightGap = clockwise;
            right = index;
        }
        if (counter < leftGap) {
            leftGap = counter;
            left = index;
        }
    }
    if (!left.has_value() || !right.has_value()) return;
    const double span = leftGap + rightGap;
    if (*left == *right || span <= 1e-4) {
        gains[*left] = 1.0;
        return;
    }
    const double position = leftGap / span;
    gains[*left] = std::cos(position * std::numbers::pi / 2.0);
    gains[*right] = std::sin(position * std::numbers::pi / 2.0);
}

void PanGains(const GeomLayout& layout, const Vector& local, double distance, double radius, double (&gains)[GEOM_MAX_CHANNELS]) {
    std::uint32_t fullBand = 0;
    for (std::uint32_t index = 0; index < layout.count; index++) {
        if (!layout.channels[index].lfe) fullBand++;
    }
    const double horizontal = std::hypot(local[0], local[2]);
    double weight = distance > 1e-6 ? horizontal / distance : 0.0;
    if (radius > 0.0) weight *= 1.0 - std::clamp(radius / std::max(distance, 1e-6), 0.0, 1.0);
    weight = std::clamp(weight, 0.0, 1.0);
    double directional[GEOM_MAX_CHANNELS] = {};
    if (weight > 0.0) {
        const double azimuth = std::atan2(local[0], local[2]) * 180.0 / std::numbers::pi;
        if (fullBand == 1) {
            for (std::uint32_t index = 0; index < layout.count; index++) {
                if (!layout.channels[index].lfe) directional[index] = 1.0;
            }
        } else if (fullBand == 2) {
            const double position = std::sin(azimuth * std::numbers::pi / 180.0);
            const double angle = (position + 1.0) * std::numbers::pi / 4.0;
            std::uint32_t first = 0;
            std::uint32_t second = 0;
            bool haveFirst = false;
            for (std::uint32_t index = 0; index < layout.count; index++) {
                if (layout.channels[index].lfe) continue;
                if (!haveFirst) {
                    first = index;
                    haveFirst = true;
                } else {
                    second = index;
                }
            }
            directional[first] = std::cos(angle);
            directional[second] = std::sin(angle);
        } else {
            PairwiseGains(layout, azimuth, directional);
        }
    }
    const double even = fullBand > 0 ? 1.0 / fullBand : 0.0;
    for (std::uint32_t index = 0; index < layout.count; index++) {
        if (layout.channels[index].lfe) {
            gains[index] = 1.0;
            continue;
        }
        gains[index] = std::sqrt(weight * directional[index] * directional[index] + (1.0 - weight) * even);
    }
}

double DistanceGain(const Ngs2GeomRolloff& rolloff, double distance) {
    const double reference = rolloff.reference_distance > 0.0f ? rolloff.reference_distance : 1.0;
    const double maximum = std::max<double>(rolloff.max_distance, reference);
    const double clamped = std::clamp(distance, reference, maximum);
    const double factor = std::max<double>(rolloff.rolloff_factor, 0.0);
    double gain = 0.0;
    if (rolloff.model == 1) {
        gain = maximum > reference ? 1.0 - factor * (clamped - reference) / (maximum - reference) : 1.0;
    } else if (rolloff.model == 2) {
        gain = std::pow(clamped / reference, -factor);
    } else {
        gain = reference / (reference + factor * (clamped - reference));
    }
    return std::clamp(gain, 0.0, 1.0);
}

double ConeGain(const Ngs2GeomListenerWork& work, const Ngs2GeomSourceParam& source, const Vector& local, double distance) {
    if (distance <= 1e-6) return source.cone.inner_level;
    const auto direction = TryUnit(RotateVector(work, source.direction));
    if (!direction.has_value()) return source.cone.inner_level;
    const Vector toListener{-local[0] / distance, -local[1] / distance, -local[2] / distance};
    const double cosine = std::clamp(Dot(*direction, toListener), -1.0, 1.0);
    const double aperture = 2.0 * std::acos(cosine) * 180.0 / std::numbers::pi;
    const double inner = std::clamp<double>(source.cone.inner_angle, 0.0, 360.0);
    const double outer = std::clamp<double>(std::max<double>(source.cone.outer_angle, inner), 0.0, 360.0);
    if (aperture <= inner) return source.cone.inner_level;
    if (aperture >= outer || outer <= inner) return source.cone.outer_level;
    const double position = (aperture - inner) / (outer - inner);
    return source.cone.inner_level + (source.cone.outer_level - source.cone.inner_level) * position;
}

double DopplerRatio(const Ngs2GeomListenerWork& work, const Ngs2GeomSourceParam& source, const Vector& local, double distance) {
    const double factor = source.doppler_factor;
    if (factor <= 0.0 || distance <= 1e-6) return 1.0;
    const double speed = work.sound_speed;
    const Vector axis{-local[0] / distance, -local[1] / distance, -local[2] / distance};
    const double limit = speed / factor;
    const double listenerSpeed = std::min(Dot(RotateVector(work, work.velocity), axis), limit * 0.999);
    const double sourceSpeed = std::min(Dot(RotateVector(work, source.velocity), axis), limit * 0.999);
    const double ratio = (speed - factor * listenerSpeed) / (speed - factor * sourceSpeed);
    if (!std::isfinite(ratio)) return 1.0;
    return std::clamp(ratio, 1.0 / 16.0, 16.0);
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

int Ngs2GeomApply(const Ngs2GeomListenerWork* listener, const Ngs2GeomSourceParam* source, Ngs2GeomAttribute* out_attrib, std::uint32_t flags) {
    if (listener == nullptr || source == nullptr) APS5_INVALID_ARG_EX;
    if (out_attrib == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if ((flags & ~GEOM_SUPPORTED_FLAGS) != 0) throw std::runtime_error("NGS2: unknown geometry apply flags");
    if (flags == 0) flags = GEOM_FLAG_DEFAULT;
    RequireFinite(*listener, *source);
    const Vector local = TransformPoint(*listener, source->position);
    const double distance = std::hypot(local[0], local[1], local[2]);
    const double level = std::clamp(DistanceGain(source->rolloff, distance) * ConeGain(*listener, *source, local, distance),
        std::min<double>(source->min_level, source->max_level), static_cast<double>(source->max_level));
    Ngs2GeomAttribute attribute{};
    if ((flags & GEOM_FLAG_SOURCE_DOPPLER) != 0) {
        attribute.pitch_ratio = static_cast<float>(DopplerRatio(*listener, *source, local, distance));
    }
    if ((flags & (GEOM_FLAG_SOURCE_ANGLE | GEOM_FLAG_A3D_ATTRIBUTE)) != 0) {
        attribute.a3d_attrib.position = {static_cast<float>(local[0]), static_cast<float>(local[1]), static_cast<float>(local[2])};
        attribute.a3d_attrib.volume = static_cast<float>(level);
    }
    if ((flags & (GEOM_FLAG_SOURCE_ANGLE | GEOM_FLAG_VOLUME_MATRIX)) != 0) {
        const GeomLayout layout = SpeakerLayout(source->num_speakers);
        double gains[GEOM_MAX_CHANNELS] = {};
        PanGains(layout, local, distance, source->radius, gains);
        for (std::uint32_t index = 0; index < layout.count; index++) {
            gains[index] *= level * (layout.channels[index].lfe ? source->lfe_level : source->fbw_level);
        }
        const std::uint32_t inputs = std::clamp<std::uint32_t>(source->matrix_format, 1, GEOM_MAX_CHANNELS);
        for (std::uint32_t input = 0; input < inputs; input++) {
            for (std::uint32_t output = 0; output < GEOM_MAX_CHANNELS; output++) {
                attribute.level[input * GEOM_MAX_CHANNELS + output] = static_cast<float>(gains[output]);
            }
        }
    }
    *out_attrib = attribute;
    return SCE_NGS2_OK;
}
