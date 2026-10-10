#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

struct GuestVbapParams {
    const AudioOut2Position* positions;
    std::uint32_t numSpeakers;
    std::uint8_t is3d;
    void* memory;
    std::size_t memorySize;
    std::uint32_t reserved;
};

constexpr std::uint32_t MaxSpeakers = 32;
constexpr std::uint32_t AmbisonicsChannelFlag = 0x40;
constexpr std::uint32_t MaxAmbisonicsOrder = 5;

struct SpeakerArray {
    std::uint32_t magic;
    std::uint32_t numSpeakers;
    AudioOut2Position directions[MaxSpeakers];
};
constexpr std::uint32_t SpeakerArrayMagic = 0x53504b41;

SpeakerArray& Array(AudioOut2SpeakerArrayHandle handle) {
    auto* array = static_cast<SpeakerArray*>(handle);
    if (array == nullptr || array->magic != SpeakerArrayMagic) throw std::invalid_argument("invalid speaker array handle");
    return *array;
}

float Sn3d(std::uint32_t acn, const AudioOut2Position& direction) {
    const int n = static_cast<int>(std::sqrt(static_cast<double>(acn)));
    const int m = static_cast<int>(acn) - n * n - n;
    const int am = std::abs(m);
    const double length = std::sqrt(double(direction.x) * direction.x + double(direction.y) * direction.y + double(direction.z) * direction.z);
    const double sinEl = length > 0 ? direction.z / length : 0;
    const double cosEl = std::sqrt(std::max(0.0, 1 - sinEl * sinEl));
    const double azimuth = std::atan2(direction.y, direction.x);
    double legendre = 1;
    for (int i = 1; i <= am; ++i) legendre *= (2 * i - 1) * cosEl;
    if (n > am) {
        double previous = legendre;
        double current = sinEl * (2 * am + 1) * legendre;
        for (int l = am + 2; l <= n; ++l) {
            const double next = (sinEl * (2 * l - 1) * current - (l + am - 1) * previous) / (l - am);
            previous = current;
            current = next;
        }
        legendre = current;
    }
    double ratio = 1;
    for (int i = n - am + 1; i <= n + am; ++i) ratio /= i;
    const double norm = std::sqrt((m == 0 ? 1.0 : 2.0) * ratio);
    return static_cast<float>(norm * legendre * (m < 0 ? std::sin(am * azimuth) : std::cos(am * azimuth)));
}

}

extern "C" {

int APS5_VABI sceAudioOut2SpeakerArrayCreate(AudioOut2SpeakerArrayHandle* handle, const void* vbap_params, const void* ambi_params) {
    const auto* params = static_cast<const GuestVbapParams*>(vbap_params);
    if (handle == nullptr || params == nullptr || ambi_params == nullptr || params->positions == nullptr ||
        params->numSpeakers == 0 || params->numSpeakers > MaxSpeakers || params->memory == nullptr ||
        params->memorySize < sizeof(SpeakerArray) || reinterpret_cast<std::uintptr_t>(params->memory) % alignof(SpeakerArray) != 0) {
        APS5_INVALID_ARG_EX;
    }
    auto* array = new (params->memory) SpeakerArray{SpeakerArrayMagic, params->numSpeakers, {}};
    std::copy_n(params->positions, params->numSpeakers, array->directions);
    *handle = array;
    return 0;
}

int APS5_VABI sceAudioOut2SpeakerArrayDestroy(AudioOut2SpeakerArrayHandle handle) {
    Array(handle).magic = 0;
    return 0;
}

int APS5_VABI sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(AudioOut2SpeakerArrayHandle handle, uint32_t ambisonics_channel, float* coefficients, uint32_t num_coefficients) {
    const auto& array = Array(handle);
    if (coefficients == nullptr || num_coefficients != array.numSpeakers) APS5_INVALID_ARG_EX;
    const std::uint32_t acn = ambisonics_channel & ~AmbisonicsChannelFlag;
    if ((ambisonics_channel & AmbisonicsChannelFlag) == 0 || acn >= (MaxAmbisonicsOrder + 1) * (MaxAmbisonicsOrder + 1)) {
        throw std::invalid_argument("unsupported ambisonics channel " + std::to_string(ambisonics_channel));
    }
    const auto order = static_cast<std::uint32_t>(std::sqrt(static_cast<double>(acn)));
    const std::uint32_t decodable = std::max<std::uint32_t>(1, (array.numSpeakers - 1) / 2);
    for (std::uint32_t speaker = 0; speaker < array.numSpeakers; ++speaker) {
        coefficients[speaker] = order > decodable ? 0.0f : Sn3d(acn, array.directions[speaker]) / static_cast<float>(array.numSpeakers);
    }
    return 0;
}

int APS5_VABI sceAudioOut2GetSpeakerArrayCoefficients(AudioOut2SpeakerArrayHandle handle, AudioOut2Position pos, float spread, float* coefficients, uint32_t num_coefficients, uint8_t height_aware, float downmix_spread_radius) {
    (void)handle;
    (void)pos;
    (void)spread;
    (void)coefficients;
    (void)num_coefficients;
    (void)height_aware;
    (void)downmix_spread_radius;
    throw std::runtime_error("sceAudioOut2GetSpeakerArrayCoefficients: not implemented");
}

size_t APS5_VABI sceAudioOut2GetSpeakerArrayMemorySize(uint32_t num_speakers, uint8_t is_3d, uint8_t is_ambisonics) {
    (void)is_3d;
    (void)is_ambisonics;
    if (num_speakers == 0 || num_speakers > MaxSpeakers) APS5_INVALID_ARG_EX;
    return sizeof(SpeakerArray);
}

int APS5_VABI sceAudioOut2GetSpeakerInfo(AudioOut2SpeakerInfo* info, uint32_t flags) {
    (void)flags;
    if (!info) return static_cast<int>(0x80260502);
    constexpr std::uint8_t SpeakerTypeStereo = 0;
    constexpr std::uint32_t FrontLeftAndRight = 0x3;
    *info = AudioOut2SpeakerInfo{};
    info->type = SpeakerTypeStereo;
    info->available_bits = FrontLeftAndRight;
    info->speaker_angle[0] = {-30, 0};
    info->speaker_angle[1] = {30, 0};
    return 0;
}

}
