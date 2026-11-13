#include "prx/libSceAudioOut/libSceAudioOut.hpp"
#include "SceTypes.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int ErrorInvalidParam = static_cast<int>(0x80268001u);
constexpr std::uint32_t KindAcn = 0;
constexpr std::uint32_t KindFuma = 1;
constexpr float Sentinel = -7.0f;
constexpr double Tolerance = 1e-5;

struct EncodeParam {
    std::uint32_t kind;
    std::int32_t order;
};

using Output = std::array<float, 40>;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

bool Near(float value, double expected) {
    return std::fabs(static_cast<double>(value) - expected) <= Tolerance;
}

int Encode(std::uint32_t kind, std::int32_t order, AudioOut2Position direction, Output& out) {
    out.fill(Sentinel);
    const EncodeParam param{kind, order};
    return sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, out.data(), direction);
}

bool Untouched(const Output& out, std::size_t from) {
    for (std::size_t index = from; index < out.size(); ++index) {
        if (out[index] != Sentinel) return false;
    }
    return true;
}

std::array<double, 16> ReferenceToOrder3(double x, double y, double z) {
    const double s3 = std::sqrt(3.0);
    return {
        1.0, y, z, x,
        s3 * x * y, s3 * y * z, (3 * z * z - 1) / 2, s3 * x * z, s3 / 2 * (x * x - y * y),
        std::sqrt(5.0 / 8.0) * y * (3 * x * x - y * y), std::sqrt(15.0) * x * y * z, std::sqrt(3.0 / 8.0) * y * (5 * z * z - 1),
        z * (5 * z * z - 3) / 2, std::sqrt(3.0 / 8.0) * x * (5 * z * z - 1), std::sqrt(15.0) / 2 * z * (x * x - y * y), std::sqrt(5.0 / 8.0) * x * (x * x - 3 * y * y),
    };
}

void CheckAcn() {
    Output out;
    Require(Encode(KindAcn, 1, {0, 0, 1}, out) == 0, "kind 0 order 1 failed for the third axis");
    Require(Near(out[0], 1) && Near(out[1], 0) && Near(out[2], 0) && Near(out[3], 1) && Untouched(out, 4), "the third float is not the ambisonic X axis");
    Require(Encode(KindAcn, 1, {1, 0, 0}, out) == 0 && Near(out[1], -1) && Near(out[2], 0) && Near(out[3], 0), "the first float is not the negative ambisonic Y axis");
    Require(Encode(KindAcn, 1, {0, 1, 0}, out) == 0 && Near(out[1], 0) && Near(out[2], 1) && Near(out[3], 0), "the second float is not the ambisonic Z axis");
    Require(Encode(KindAcn, 2, {-1, 0, 1}, out) == 0 && Near(out[4], std::sqrt(3.0) / 2) && Untouched(out, 9), "ACN 4 at 45 degrees left is not sqrt(3)/2");

    const double length = std::sqrt(29.0);
    const double x = 2 / length;
    const double y = -3 / length;
    const double z = -4 / length;
    Require(Encode(KindAcn, 5, {3, -4, 2}, out) == 0, "kind 0 order 5 failed");
    const auto reference = ReferenceToOrder3(x, y, z);
    for (std::size_t channel = 0; channel < reference.size(); ++channel) {
        Require(Near(out[channel], reference[channel]), "an ACN channel up to order 3 is not the SN3D harmonic");
    }
    const double z2 = z * z;
    Require(Near(out[20], (35 * z2 * z2 - 30 * z2 + 3) / 8), "ACN 20 is not the SN3D harmonic");
    Require(Near(out[24], std::sqrt(35.0) / 8 * (x * x * x * x - 6 * x * x * y * y + y * y * y * y)), "ACN 24 is not the SN3D harmonic");
    const double fifth = 3 * std::sqrt(14.0) / 16;
    Require(Near(out[25], fifth * (5 * x * x * x * x * y - 10 * x * x * y * y * y + y * y * y * y * y)), "ACN 25 is not the SN3D harmonic");
    Require(Near(out[30], z * (63 * z2 * z2 - 70 * z2 + 15) / 8), "ACN 30 is not the SN3D harmonic");
    Require(Near(out[35], fifth * (x * x * x * x * x - 10 * x * x * x * y * y + 5 * x * y * y * y * y)), "ACN 35 is not the SN3D harmonic");
    Require(Untouched(out, 36), "kind 0 order 5 wrote past 36 floats");

    Output scaled;
    Require(Encode(KindAcn, 5, {0.3f, -0.4f, 0.2f}, scaled) == 0, "kind 0 order 5 failed for a short direction");
    for (std::size_t channel = 0; channel < 36; ++channel) {
        Require(std::fabs(scaled[channel] - out[channel]) <= 1e-6f, "the direction is not normalised");
    }

    Require(Encode(KindAcn, 2, {0, 0, 0}, out) == 0 && out[0] == 1.0f, "a zero direction did not write 1 to the first channel");
    for (std::size_t channel = 1; channel < 9; ++channel) Require(out[channel] == 0.0f, "a zero direction did not clear the other channels");

    for (const std::int32_t order : {6, 7, 8, 10, -1, -2}) {
        Require(Encode(KindAcn, order, {3, -4, 2}, out) == 0 && Untouched(out, 0), "an unsupported kind 0 order did not return 0 without writing");
    }
}

void CheckFuma() {
    struct Channel {
        std::size_t acn;
        double weight;
    };
    const std::array<Channel, 16> fuma{{
        {0, 1 / std::sqrt(2.0)}, {3, 1}, {1, 1}, {2, 1},
        {6, 1}, {7, 2 / std::sqrt(3.0)}, {5, 2 / std::sqrt(3.0)}, {8, 2 / std::sqrt(3.0)}, {4, 2 / std::sqrt(3.0)},
        {12, 1}, {13, std::sqrt(45.0 / 32.0)}, {11, std::sqrt(45.0 / 32.0)}, {14, 3 / std::sqrt(5.0)}, {10, 3 / std::sqrt(5.0)}, {15, std::sqrt(8.0 / 5.0)}, {9, std::sqrt(8.0 / 5.0)},
    }};
    const double length = std::sqrt(29.0);
    const auto reference = ReferenceToOrder3(2 / length, -3 / length, -4 / length);
    Output out;
    Require(Encode(KindFuma, 3, {3, -4, 2}, out) == 0, "kind 1 order 3 failed");
    for (std::size_t channel = 0; channel < fuma.size(); ++channel) {
        Require(Near(out[channel], fuma[channel].weight * reference[fuma[channel].acn]), "a FuMa channel has the wrong harmonic or weight");
    }
    Require(Untouched(out, 16), "kind 1 order 3 wrote past 16 floats");
    Require(Encode(KindFuma, 1, {3, -4, 2}, out) == 0 && Untouched(out, 4), "kind 1 order 1 wrote past 4 floats");

    Require(Encode(KindFuma, 3, {0, 0, 0}, out) == 0 && Near(out[0], 1 / std::sqrt(2.0)), "a zero direction did not write W's weight");
    for (std::size_t channel = 1; channel < 16; ++channel) Require(out[channel] == 0.0f, "a zero direction did not clear the other FuMa channels");

    Require(Encode(KindFuma, 4, {3, -4, 2}, out) == ErrorInvalidParam && Untouched(out, 0), "kind 1 order 4 was accepted");
}

void CheckArguments() {
    Output out;
    out.fill(Sentinel);
    const EncodeParam param{KindAcn, 1};
    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(nullptr, out.data(), {0, 0, 1}) == ErrorInvalidParam, "null parameters were accepted");
    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, nullptr, {0, 0, 1}) == ErrorInvalidParam, "a null output was accepted");
    Require(Encode(2, 1, {0, 0, 1}, out) == ErrorInvalidParam && Untouched(out, 0), "kind 2 was accepted");
}

}

int main() {
    CheckAcn();
    CheckFuma();
    CheckArguments();
    std::puts("AudioOut2 ambisonics encode tests passed");
    return 0;
}
