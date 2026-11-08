#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Ngs2Internal.hpp"

extern "C" {
#include "coding/coding.h"
}

static constexpr std::uint64_t VAG_FRAME_SAMPLES = 28;
static constexpr std::size_t VAG_FRAME_BYTES = 16;
static constexpr std::uint32_t VAG_MAX_PREDICTOR = 127;
static constexpr std::uint32_t VAG_MAX_SHIFT = 12;

struct Ngs2VagFrameStream {
    STREAMFILE stream{};
    const std::uint8_t* frame = nullptr;
};

static std::size_t ReadVagFrame(STREAMFILE* stream, std::uint8_t* destination, offv_t offset, std::size_t length) {
    if (offset < 0 || static_cast<std::size_t>(offset) >= VAG_FRAME_BYTES) return 0;
    length = std::min(length, VAG_FRAME_BYTES - static_cast<std::size_t>(offset));
    std::memcpy(destination, reinterpret_cast<Ngs2VagFrameStream*>(stream)->frame + offset, length);
    return length;
}

static std::uint64_t VagFrames(const Ngs2WaveformBlock& block) {
    return (static_cast<std::uint64_t>(block.num_skip_samples) + block.num_samples + VAG_FRAME_SAMPLES - 1) / VAG_FRAME_SAMPLES;
}

std::size_t Ngs2VagBlockBytes(std::uint32_t channels, const Ngs2WaveformBlock& block) {
    return static_cast<std::size_t>(VagFrames(block) * VAG_FRAME_BYTES * channels);
}

void Ngs2CheckVag(std::uint32_t channels, const std::uint8_t* data, const Ngs2WaveformBlock& block) {
    const std::uint64_t frames = VagFrames(block);
    for (std::uint64_t frame = 0; frame < frames * channels; frame++) {
        const auto* unit = data + frame * VAG_FRAME_BYTES;
        const std::uint32_t predictor = (unit[1] & 0xf0u) | (unit[0] >> 4);
        if (predictor > VAG_MAX_PREDICTOR || (unit[0] & 0xfu) > VAG_MAX_SHIFT) {
            throw std::runtime_error("NGS2: VAG frame " + std::to_string(frame) + " with predictor " + std::to_string(predictor) + " and shift " + std::to_string(unit[0] & 0xfu) + " is not implemented");
        }
    }
}

static std::size_t BytesBefore(const Ngs2Voice& voice, std::uint64_t position) {
    return static_cast<std::size_t>(position / VAG_FRAME_SAMPLES * VAG_FRAME_BYTES * voice.channels);
}

void Ngs2DecodeVag(Ngs2Voice& voice, Ngs2Block& block) {
    const std::uint64_t frames = VagFrames(block.info);
    const std::uint32_t channels = voice.channels;
    if (block.data != voice.vag.next) voice.vag.history = {};
    auto pcm = std::make_shared<std::vector<std::int16_t>>(static_cast<std::size_t>(frames * VAG_FRAME_SAMPLES * channels), 0);
    Ngs2VagFrameStream input{};
    input.stream.read = ReadVagFrame;
    for (std::uint32_t channel = 0; channel < channels; channel++) {
        auto& history = voice.vag.history[channel];
        VGMSTREAMCHANNEL decoder{};
        decoder.streamfile = &input.stream;
        decoder.adpcm_history1_32 = history[0];
        decoder.adpcm_history2_32 = history[1];
        decoder.adpcm_history3_32 = history[2];
        decoder.adpcm_history4_32 = history[3];
        for (std::uint64_t frame = 0; frame < frames; frame++) {
            input.frame = block.data + (frame * channels + channel) * VAG_FRAME_BYTES;
            decode_hevag(&decoder, pcm->data() + frame * VAG_FRAME_SAMPLES * channels + channel, static_cast<int>(channels), 0, static_cast<std::int32_t>(VAG_FRAME_SAMPLES));
        }
        history = {decoder.adpcm_history1_32, decoder.adpcm_history2_32, decoder.adpcm_history3_32, decoder.adpcm_history4_32};
    }
    block.pcm = std::move(pcm);
    voice.vag.next = block.data + frames * VAG_FRAME_BYTES * channels;
}

std::uint32_t Ngs2ReadSamplerVag(Ngs2Voice& voice, float* dest, std::uint32_t stride, std::uint32_t count) {
    std::uint32_t written = 0;
    while (written < count && !voice.blocks.empty()) {
        auto& block = voice.blocks.front();
        if (!block.started) {
            Ngs2BindSamplerBlock(voice, false);
            if (block.info.num_samples == 0) block.started = true;
        }
        if (voice.pcmRemaining == 0) {
            Ngs2FinishBlock(voice);
            continue;
        }
        const std::uint64_t position = static_cast<std::uint64_t>(block.info.num_skip_samples) + block.info.num_samples - voice.pcmRemaining;
        const auto* frame = block.pcm->data() + position * voice.channels;
        for (std::uint32_t channel = 0; channel < voice.channels; ++channel) dest[static_cast<std::size_t>(channel) * stride + written] = frame[channel] / 32768.0f;
        ++written;
        --voice.pcmRemaining;
        if (position >= block.info.num_skip_samples) ++voice.decodedSamples;
        const auto cursor = BytesBefore(voice, position + 1);
        voice.decodedBytes += cursor - block.dataCursor;
        block.dataCursor = cursor;
        if (voice.pcmRemaining != 0 && written == count) voice.waveformData = block.data + block.dataCursor;
        if (voice.pcmRemaining == 0) Ngs2FinishBlock(voice);
    }
    return written;
}
