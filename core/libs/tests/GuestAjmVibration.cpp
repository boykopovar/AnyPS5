#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

extern "C" {
int APS5_VABI sceAjmInitialize(std::int64_t, std::uint32_t*);
int APS5_VABI sceAjmFinalize(std::uint32_t);
int APS5_VABI sceAjmDecAt9ParseConfigData(const void*, AjmDecAt9ConfigDataInfo*);
int APS5_VABI sceAjmInstanceCreate(std::uint32_t, std::uint32_t, std::uint64_t, std::uint32_t*);
int APS5_VABI sceAjmInstanceDestroy(std::uint32_t, std::uint32_t);
int APS5_VABI sceAjmBatchInitialize(void*, std::size_t, AjmBatchInfo*);
int APS5_VABI sceAjmBatchJobInitialize(AjmBatchInfo*, std::uint32_t, const void*, std::size_t, void*);
int APS5_VABI sceAjmBatchJobRun(AjmBatchInfo*, std::uint32_t, std::uint64_t, const void*, std::size_t, void*, std::size_t, void*, std::size_t);
int APS5_VABI sceAjmBatchStart(std::uint32_t, const AjmBatchInfo*, int, AjmBatchError*, std::uint32_t*);
int APS5_VABI sceAjmBatchWait(std::uint32_t, std::uint32_t, std::uint32_t, AjmBatchError*);
}

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void Submit(std::uint32_t context, const AjmBatchInfo& info) {
    AjmBatchError error{};
    std::uint32_t id = 0;
    Require(sceAjmBatchStart(context, &info, 0, &error, &id) == 0 && sceAjmBatchWait(context, id, 0, &error) == 0, "batch submit");
}

std::array<std::uint8_t, 4> Config(unsigned channels) {
    return {0xFE, static_cast<std::uint8_t>(channels == 1 ? 0x7C : 0x7E), 0x03, 0xF0};
}

std::vector<std::uint8_t> Packet(const std::vector<int>& amplitudes) {
    std::vector<std::uint8_t> bytes;
    std::size_t position = 0;
    const auto put = [&](unsigned value, unsigned bits) {
        for (unsigned bit = bits; bit > 0; --bit) {
            if (position / 8 == bytes.size()) bytes.push_back(0);
            bytes[position / 8] |= ((value >> (bit - 1)) & 1u) << (7 - position % 8);
            ++position;
        }
    };
    for (unsigned frame = 0; frame < 4; ++frame) {
        for (const auto amplitude : amplitudes) {
            put(frame != 0, 1);
            put(0, 1);
            put(0, 1);
            put(0, 4);
            put(25, 5);
            put(6, 3);
            put(static_cast<unsigned>(amplitude) & 255u, 8);
            put(static_cast<unsigned>(-amplitude) & 255u, 8);
            while (position % 8 != 0) put(0, 1);
        }
    }
    bytes.resize(128, 1);
    return bytes;
}

struct Stream {
    std::int32_t result = -1;
    std::int32_t internalResult = -1;
    std::int32_t consumed = -1;
    std::int32_t produced = -1;
    std::uint64_t samples = 0;
    std::uint32_t frames = 0;
    std::uint32_t reserved = 0;
};

template<class T>
std::vector<T> Decode(std::uint32_t context, unsigned channels, unsigned encoding, const std::vector<int>& amplitudes, unsigned profile = 7, bool partial = false, bool malformed = false) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 1, static_cast<std::uint64_t>(encoding) << 7u, &instance) == 0, "create vibration instance");
    auto config = Config(channels);
    config[1] = static_cast<std::uint8_t>((profile << 4) | (config[1] & 15u));
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    std::int32_t initialized[2] = {-1, -1};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0, "initialize batch");
    Require(sceAjmBatchJobInitialize(&info, instance, config.data(), config.size(), initialized) == 0, "append initialize");
    Submit(context, info);
    Require(initialized[0] == 0 && initialized[1] == 0, "initialize vibration decoder");
    auto packet = Packet(amplitudes);
    const auto samples = profile == 1 ? 256 : profile == 4 ? 512 : 1024;
    std::vector<T> pcm(samples * channels);
    constexpr std::uint64_t flags = (1ull << 47) | (1ull << 12);
    const auto run = [&](const std::vector<std::uint8_t>& input, std::size_t size) {
        Stream stream;
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0, "reset batch");
        Require(sceAjmBatchJobRun(&info, instance, flags, input.data(), size, pcm.data(), pcm.size() * sizeof(T), &stream, sizeof(stream)) == 0, "append run");
        Submit(context, info);
        return stream;
    };
    if (partial) {
        std::fill(pcm.begin(), pcm.end(), T{123});
        const auto stream = run(packet, packet.size() - 1);
        Require(stream.result == 8 && stream.consumed == 0 && stream.produced == 0, "partial superframe accounting");
        Require(std::all_of(pcm.begin(), pcm.end(), [](T value) { return value == T{123}; }), "partial input changed PCM");
    }
    if (malformed) {
        auto broken = packet;
        broken[0] |= 0x80;
        const auto stream = run(broken, broken.size());
        Require((stream.result & 2) != 0 && stream.consumed == 0 && stream.produced == 0, "invalid first-frame flag");
    }
    const auto stream = run(packet, packet.size());
    Require(stream.result == 0x18 && stream.internalResult == 0, "decode vibration superframe");
    Require(stream.consumed == 128 && stream.produced == static_cast<int>(pcm.size() * sizeof(T)), "consumed/produced accounting");
    Require(stream.samples == static_cast<unsigned>(samples) && stream.frames == 4, "decoded sample/frame accounting");
    Require(sceAjmInstanceDestroy(context, instance) == 0, "destroy instance");
    return pcm;
}

void Parse() {
    for (const auto index : {6u, 7u}) {
        for (const auto profile : {1u, 4u, 7u}) {
            auto config = Config(index == 6 ? 1 : 2);
            config[1] = static_cast<std::uint8_t>((profile << 4) | (index << 1));
            AjmDecAt9ConfigDataInfo info{};
            Require(sceAjmDecAt9ParseConfigData(config.data(), &info) == 0, "parse vibration config");
            const unsigned samples = profile == 1 ? 64 : profile == 4 ? 128 : 256;
            Require(info.channels == (index == 6 ? 1u : 2u) && info.sample_rate == (profile == 1 ? 12000u : profile == 4 ? 24000u : 48000u), "vibration layout/rate");
            Require(info.superframe_size == 128 && info.frame_samples_per_channel == samples && info.superframe_samples_per_channel == samples * 4, "vibration frame geometry");
        }
    }
    auto invalid = Config(2);
    invalid[1] |= 1;
    AjmDecAt9ConfigDataInfo info;
    std::memset(&info, 0xAA, sizeof(info));
    Require(sceAjmDecAt9ParseConfigData(invalid.data(), &info) == static_cast<int>(0x80930005), "invalid version accepted");
    Require(info.superframe_size == 0xAAAAAAAAu, "invalid parse changed output");
}
}

int main() {
    try {
        Parse();
        std::uint32_t context = 0;
        Require(sceAjmInitialize(0, &context) == 0, "initialize AJM");
        const auto left = Decode<float>(context, 1, 2, {1});
        const auto right = Decode<float>(context, 1, 2, {2});
        const auto stereo = Decode<float>(context, 2, 2, {1, 2}, 7, true, true);
        Require(std::any_of(left.begin(), left.end(), [](float value) { return std::abs(value) > 0.000001f; }), "vibration decoded to silence");
        for (std::size_t sample = 0; sample < left.size(); ++sample) {
            Require(std::isfinite(stereo[2 * sample]) && std::isfinite(stereo[2 * sample + 1]), "non-finite PCM");
            Require(stereo[2 * sample] == left[sample] && stereo[2 * sample + 1] == right[sample], "dual-mono interleaving or reset history");
        }
        const auto pcm16 = Decode<std::int16_t>(context, 2, 0, {1, 2});
        const auto pcm32 = Decode<std::int32_t>(context, 2, 1, {1, 2});
        for (std::size_t sample = 0; sample < stereo.size(); ++sample) {
            Require(std::abs(static_cast<double>(pcm16[sample]) / 32768.0 - stereo[sample]) <= 1.0 / 32768.0, "PCM16 conversion");
            Require(std::abs(static_cast<double>(pcm32[sample]) / 2147483648.0 - stereo[sample]) <= 1.0 / 2147483648.0, "PCM32 conversion");
        }
        for (const auto profile : {1u, 4u}) {
            const auto mono = Decode<float>(context, 1, 2, {1}, profile);
            const auto dual = Decode<float>(context, 2, 2, {1, 1}, profile);
            Require(std::any_of(mono.begin(), mono.end(), [](float value) { return std::abs(value) > 0.000001f; }), "lower-rate vibration decoded to silence");
            for (std::size_t sample = 0; sample < mono.size(); ++sample) Require(dual[2 * sample] == mono[sample] && dual[2 * sample + 1] == mono[sample], "lower-rate interleaving");
        }
        Require(sceAjmFinalize(context) == 0, "finalize AJM");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
