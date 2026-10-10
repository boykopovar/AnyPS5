#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

static constexpr float DEGREES = 360.0f;
static constexpr float RADIANS = 2.0f * std::numbers::pi_v<float>;
static constexpr std::uint32_t MAX_PAN_PARAMS = 8;
static constexpr std::uint32_t LFE_CHANNEL = 3;

struct PanLayout {
    std::uint32_t inputOrder[7];
    std::uint32_t channels[7];
    float defaultDegrees[7];
    std::uint32_t mixChannels;
};

static constexpr PanLayout STEREO_LAYOUT{{1, 0}, {1, 0}, {90, 270}, 2};
static constexpr PanLayout QUAD_LAYOUT{{1, 3, 2, 0}, {1, 5, 4, 0}, {40, 110, 250, 320}, 6};
static constexpr PanLayout FIVE_LAYOUT{{2, 1, 4, 3, 0}, {2, 1, 5, 4, 0}, {0, 40, 110, 250, 320}, 6};
static constexpr PanLayout SEVEN_LAYOUT{{2, 1, 4, 6, 5, 3, 0}, {2, 1, 5, 7, 6, 4, 0}, {0, 40, 110, 150, 210, 250, 320}, 8};

static const PanLayout* LayoutFor(std::uint32_t numSpeakers) {
    switch (numSpeakers) {
        case 2: return &STEREO_LAYOUT;
        case 4: return &QUAD_LAYOUT;
        case 5: return &FIVE_LAYOUT;
        case 7: return &SEVEN_LAYOUT;
        default: return nullptr;
    }
}

static bool IsUnitAngle(float unitAngle) {
    return unitAngle == DEGREES || unitAngle == RADIANS;
}

static float EqualPower(float position) {
    return std::sin(position * (std::numbers::pi_v<float> / 2.0f));
}

static void AddBetweenSpeakers(const Ngs2PanWork& work, std::uint32_t speaker, float angle, float weight, float* gains) {
    const float from = work.speaker_angles[speaker - 1];
    const float to = work.speaker_angles[speaker];
    if (!(angle >= from) || !(to > angle)) return;
    const float position = (angle - from) / (to - from);
    gains[speaker - 1] += EqualPower(1.0f - position) * weight;
    gains[speaker] += EqualPower(position) * weight;
}

int Ngs2PanInit(Ngs2PanWork* work, const float* speaker_angles, float unit_angle, uint32_t num_speakers) {
    if (work == nullptr) APS5_INVALID_ARG_EX;
    if (speaker_angles != nullptr && !IsUnitAngle(unit_angle)) APS5_INVALID_ARG_EX;
    const auto* layout = LayoutFor(num_speakers);
    if (layout == nullptr) APS5_INVALID_ARG_EX;
    *work = {};
    work->num_speakers = num_speakers;
    if (speaker_angles == nullptr) {
        for (std::uint32_t i = 0; i < num_speakers; i++) work->speaker_angles[i] = layout->defaultDegrees[i];
        work->speaker_angles[num_speakers] = layout->defaultDegrees[0] + DEGREES;
        work->unit_angle = DEGREES;
        if (unit_angle == RADIANS) {
            for (std::uint32_t i = 0; i < num_speakers; i++) work->speaker_angles[i] = work->speaker_angles[i] / 180.0f * std::numbers::pi_v<float>;
            work->unit_angle = RADIANS;
        }
        return SCE_NGS2_OK;
    }
    for (std::uint32_t i = 0; i < num_speakers; i++) {
        float angle = speaker_angles[layout->inputOrder[i]];
        if (!std::isfinite(angle)) APS5_INVALID_ARG_EX;
        while (angle < 0.0f) {
            if (angle + unit_angle == angle) throw std::runtime_error("NGS2: pan angles too large to wrap are not implemented");
            angle += unit_angle;
        }
        while (angle >= unit_angle) {
            if (!(unit_angle < angle)) APS5_INVALID_ARG_EX;
            if (angle - unit_angle == angle) throw std::runtime_error("NGS2: pan angles too large to wrap are not implemented");
            angle -= unit_angle;
        }
        work->speaker_angles[i] = angle;
    }
    work->speaker_angles[num_speakers] = work->speaker_angles[0] + unit_angle;
    work->unit_angle = unit_angle;
    return SCE_NGS2_OK;
}

int Ngs2PanGetVolumeMatrix(Ngs2PanWork* work, const Ngs2PanParam* params, uint32_t num_params, uint32_t matrix_format, float* out_volume_matrix) {
    if (work == nullptr || params == nullptr) APS5_INVALID_ARG_EX;
    if (out_volume_matrix == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (matrix_format != 1 && matrix_format != 2 && matrix_format != 6 && matrix_format != 8) APS5_INVALID_ARG_EX;
    const auto* layout = LayoutFor(work->num_speakers);
    if (layout == nullptr || !IsUnitAngle(work->unit_angle) || !std::isfinite(work->speaker_angles[0]) || num_params > MAX_PAN_PARAMS) APS5_INVALID_ARG_EX;
    for (std::uint32_t p = 0; p < num_params; p++) {
        if (!std::isfinite(params[p].angle) || std::isnan(params[p].distance)) APS5_INVALID_ARG_EX;
    }
    std::memset(out_volume_matrix, 0, static_cast<std::size_t>(num_params) * matrix_format * sizeof(float));
    const std::uint32_t numSpeakers = work->num_speakers;
    const float unit = work->unit_angle;
    const float first = work->speaker_angles[0];
    for (std::uint32_t p = 0; p < num_params; p++) {
        const auto& param = params[p];
        const float distance = std::max(-1.0f, std::min(1.0f, param.distance));
        const float towardWeight = std::sin((distance + 1.0f) * 0.5f * std::numbers::pi_v<float> * 0.5f);
        const float oppositeWeight = std::sin((1.0f - distance) * 0.5f * std::numbers::pi_v<float> * 0.5f);
        float angle = param.angle;
        while (angle < first || angle >= first + unit) {
            const float before = angle;
            if (angle >= first + unit) angle -= unit;
            if (angle < first) angle += unit;
            if (angle == before) throw std::runtime_error("NGS2: pan angles too large to wrap are not implemented");
        }
        float opposite = angle + unit * 0.5f;
        if (!(opposite < first + unit)) opposite -= unit;
        float gains[8] = {};
        for (std::uint32_t speaker = 1; speaker <= numSpeakers; speaker++) {
            AddBetweenSpeakers(*work, speaker, angle, towardWeight, gains);
            AddBetweenSpeakers(*work, speaker, opposite, oppositeWeight, gains);
        }
        float mix[8] = {};
        mix[layout->channels[0]] = gains[0] + gains[numSpeakers];
        for (std::uint32_t speaker = 1; speaker < numSpeakers; speaker++) mix[layout->channels[speaker]] = gains[speaker];
        for (std::uint32_t channel = 0; channel < 8; channel++) mix[channel] = channel == LFE_CHANNEL ? param.lfe_level : mix[channel] * param.fbw_level;
        for (std::uint32_t dest = 0; dest < matrix_format; dest++) {
            float& level = out_volume_matrix[static_cast<std::size_t>(dest) * num_params + p];
            for (std::uint32_t channel = 0; channel < layout->mixChannels; channel++) level += Ngs2DefaultLevel(layout->mixChannels, channel, matrix_format, dest) * mix[channel];
        }
    }
    return SCE_NGS2_OK;
}
