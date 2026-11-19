#include "prx/libSceNgs2/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <utility>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

bool CopyInPreservingErrno(void* dst, const void* src, std::size_t size) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(dst, src, size);
    errno = saved;
    return ok;
}

bool CopyOutPreservingErrno(void* dst, const void* src, std::size_t size) {
    const int saved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(dst, src, size);
    errno = saved;
    return ok;
}

template <typename T>
void StoreValue(std::vector<std::uint8_t>& buffer, std::size_t offset, T value) {
    std::memcpy(buffer.data() + offset, &value, sizeof(value));
}

void Radix2Fft(std::vector<std::complex<double>>& data, bool inverse) {
    const std::size_t n = data.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }
    const double sign = inverse ? 1.0 : -1.0;
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double angle = sign * (2.0 * std::numbers::pi) / static_cast<double>(len);
        const std::complex<double> step(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            const std::size_t half = len >> 1;
            for (std::size_t j = 0; j < half; ++j) {
                const std::complex<double> u = data[i + j];
                const std::complex<double> v = data[i + j + half] * w;
                data[i + j] = u + v;
                data[i + j + half] = u - v;
                w *= step;
            }
        }
    }
}

std::size_t InternalBinIndex(std::size_t q, std::size_t r, std::size_t j, std::size_t quarterBins) {
    switch (r) {
        case 0: return 4 * q + j;
        case 1: return (q == 0 && j == 0) ? quarterBins : (2 * quarterBins - 4 * q - j);
        case 2: return 2 * quarterBins + 4 * q + j;
        default: return (q == 0 && j == 0) ? 3 * quarterBins : (4 * quarterBins - 4 * q - j);
    }
}

void PackInternalOrder(const std::vector<std::pair<float, float>>& bins, std::size_t points, std::vector<float>& out) {
    const std::size_t quarterBins = points / 8;
    const std::size_t quadGroups = quarterBins / 4;
    out.assign(points, 0.0f);
    for (std::size_t q = 0; q < quadGroups; ++q) {
        for (std::size_t r = 0; r < 4; ++r) {
            const std::size_t base = (4 * q + r) * 8;
            for (std::size_t j = 0; j < 4; ++j) {
                const std::size_t bin = InternalBinIndex(q, r, j, quarterBins);
                out[base + j] = bins[bin].first;
                out[base + 4 + j] = bins[bin].second;
            }
        }
    }
}

void UnpackInternalOrder(const std::vector<float>& in, std::size_t points, std::vector<std::pair<float, float>>& bins) {
    const std::size_t quarterBins = points / 8;
    const std::size_t quadGroups = quarterBins / 4;
    bins.assign(points / 2, {0.0f, 0.0f});
    for (std::size_t q = 0; q < quadGroups; ++q) {
        for (std::size_t r = 0; r < 4; ++r) {
            const std::size_t base = (4 * q + r) * 8;
            for (std::size_t j = 0; j < 4; ++j) {
                const std::size_t bin = InternalBinIndex(q, r, j, quarterBins);
                bins[bin] = {in[base + j], in[base + 4 + j]};
            }
        }
    }
}

void PackNaturalOrder(const std::vector<std::pair<float, float>>& bins, std::size_t points, std::vector<float>& out) {
    const std::size_t quarterBins = points / 8;
    const std::size_t blockStride = points / 2;
    out.assign(points * 2, 0.0f);
    for (std::size_t q = 0; q < 4; ++q) {
        const std::size_t binStart = q * quarterBins;
        const std::size_t blockBase = q * blockStride;
        for (std::size_t r = 0; r < quarterBins; ++r) {
            const std::size_t dstOffset = ((q & 1u) != 0 && r > 0) ? (quarterBins - r) : r;
            const auto& bin = bins[binStart + r];
            out[blockBase + dstOffset * 2] = bin.first;
            out[blockBase + dstOffset * 2 + 1] = bin.second;
        }
    }
}

} // namespace

extern "C" {

int APS5_VABI sceNgs2FftInit(void* work, std::size_t size, int length, std::uint32_t flags, void** handle) {
    static constexpr std::array<std::uint32_t, 5> kSamples = {64, 128, 256, 512, 1024};
    if (static_cast<std::uint32_t>(length - 8) > 4u || work == nullptr || handle == nullptr) return -1;
    std::uint32_t count = kSamples[static_cast<std::size_t>(length - 8)];
    if ((flags & 2u) != 0) count /= 2u;
    const std::size_t required = static_cast<std::size_t>(count) * 32u + 64u;
    if (size < required) return -1;

    std::vector<std::uint8_t> image(required, 0);
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(work);
    const std::uintptr_t tableAddr = (base + 0x57u) & ~std::uintptr_t{0x3fu};
    const std::size_t tableOffset = static_cast<std::size_t>(tableAddr - base);
    const std::uintptr_t secondAddr = tableAddr + ((flags & 2u) != 0 ? static_cast<std::size_t>(count) * 16u : 0u);
    StoreValue<std::uint64_t>(image, 0x00, static_cast<std::uint64_t>(tableAddr));
    StoreValue<std::uint64_t>(image, 0x08, static_cast<std::uint64_t>(secondAddr));
    StoreValue<std::int32_t>(image, 0x10, static_cast<std::int32_t>(length));
    StoreValue<std::uint32_t>(image, 0x14, flags);

    if (tableOffset < required) {
        const std::size_t points = std::size_t{1} << length;
        const std::size_t maxFloats = (required - tableOffset) / sizeof(float);
        for (std::size_t index = 0; index + 1 < maxFloats; index += 2) {
            const std::size_t k = index / 2;
            const double angle = (-2.0 * std::numbers::pi * static_cast<double>(k)) / static_cast<double>(points);
            StoreValue<float>(image, tableOffset + index * sizeof(float), static_cast<float>(std::cos(angle)));
            StoreValue<float>(image, tableOffset + (index + 1) * sizeof(float), static_cast<float>(std::sin(angle)));
        }
    }

    if (!CopyOutPreservingErrno(work, image.data(), image.size())) return -1;
    void* outHandle = work;
    if (!CopyOutPreservingErrno(handle, &outHandle, sizeof(outHandle))) return -1;
    return 0;
}

int APS5_VABI sceNgs2FftProcess(void* handle, const float* in, float* out, float* work, std::uintptr_t unused, std::uint32_t mode) {
    (void)unused;
    if (handle == nullptr || in == nullptr || out == nullptr) return -1;
    std::int32_t length = 0;
    std::uint32_t flags = 0;
    const auto* header = static_cast<const std::uint8_t*>(handle);
    if (!CopyInPreservingErrno(&length, header + 0x10, sizeof(length)) ||
        !CopyInPreservingErrno(&flags, header + 0x14, sizeof(flags)) ||
        static_cast<std::uint32_t>(length - 8) > 4u) {
        return -1;
    }

    const std::size_t points = std::size_t{1} << length;
    if ((flags & 2u) == 0) {
        const std::size_t totalFloats = points * 2u;
        std::vector<float> dummyIn(totalFloats, 0.0f);
        if (!CopyInPreservingErrno(dummyIn.data(), in, dummyIn.size() * sizeof(float))) return -1;
        std::vector<float> copied(totalFloats, 0.0f);
        if (work != nullptr && !CopyInPreservingErrno(copied.data(), work, copied.size() * sizeof(float))) return -1;
        if (!CopyOutPreservingErrno(out, copied.data(), copied.size() * sizeof(float))) return -1;
        return 0;
    }

    if (work != nullptr) {
        std::vector<float> scratch(points, 0.0f);
        if (!CopyInPreservingErrno(scratch.data(), work, scratch.size() * sizeof(float)) ||
            !CopyOutPreservingErrno(work, scratch.data(), scratch.size() * sizeof(float))) {
            return -1;
        }
    }

    const float scale = 1.0f / static_cast<float>(points);
    if ((mode & 1u) != 0) {
        std::vector<float> input(points, 0.0f);
        if (!CopyInPreservingErrno(input.data(), in, input.size() * sizeof(float))) return -1;
        std::vector<std::complex<double>> spectrum(points);
        for (std::size_t n = 0; n < points; ++n) spectrum[n] = {static_cast<double>(input[n]), 0.0};
        Radix2Fft(spectrum, false);

        const std::size_t half = points / 2;
        std::vector<std::pair<float, float>> bins(half);
        bins[0] = {static_cast<float>(spectrum[0].real()), static_cast<float>(spectrum[half].real())};
        for (std::size_t k = 1; k < half; ++k) {
            bins[k] = {static_cast<float>(spectrum[k].real()), static_cast<float>(spectrum[k].imag())};
        }

        std::vector<float> output;
        if ((mode & 4u) != 0) {
            PackInternalOrder(bins, points, output);
            if ((mode & 8u) != 0) {
                for (float& value : output) value *= scale;
            }
        } else {
            PackNaturalOrder(bins, points, output);
            if ((mode & 8u) != 0) {
                for (std::size_t i = 0; i < points; ++i) output[i] *= scale;
            }
        }
        if (!CopyOutPreservingErrno(out, output.data(), output.size() * sizeof(float))) return -1;
        return 0;
    }

    std::vector<float> input(points, 0.0f);
    if (!CopyInPreservingErrno(input.data(), in, input.size() * sizeof(float))) return -1;
    std::vector<std::pair<float, float>> bins;
    if ((mode & 4u) != 0) {
        UnpackInternalOrder(input, points, bins);
    } else {
        const std::size_t half = points / 2;
        bins.resize(half);
        for (std::size_t k = 0; k < half; ++k) bins[k] = {input[k * 2], input[k * 2 + 1]};
    }

    const std::size_t half = points / 2;
    std::vector<std::complex<double>> spectrum(points);
    spectrum[0] = {static_cast<double>(bins[0].first), 0.0};
    spectrum[half] = {static_cast<double>(bins[0].second), 0.0};
    for (std::size_t k = 1; k < half; ++k) {
        const std::complex<double> value(static_cast<double>(bins[k].first), static_cast<double>(bins[k].second));
        spectrum[k] = value;
        spectrum[points - k] = std::conj(value);
    }
    Radix2Fft(spectrum, true);

    std::vector<float> output(points, 0.0f);
    for (std::size_t n = 0; n < points; ++n) {
        float value = static_cast<float>(spectrum[n].real());
        if ((mode & 8u) != 0) value *= scale;
        output[n] = value;
    }
    if (!CopyOutPreservingErrno(out, output.data(), output.size() * sizeof(float))) return -1;
    return 0;
}

}
