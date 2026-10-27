#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "libatrac9.h"
#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

static constexpr std::uint16_t WAVE_FORMAT_PCM = 0x1;
static constexpr std::uint16_t WAVE_FORMAT_IEEE_FLOAT = 0x3;
static constexpr std::uint16_t WAVE_FORMAT_EXTENSIBLE = 0xfffe;
static constexpr std::uint8_t ATRAC9_GUID[16] = {0xd2, 0x42, 0xe1, 0x47, 0xba, 0x36, 0x8d, 0x4d, 0x88, 0xfc, 0x61, 0x65, 0x4f, 0x8c, 0x83, 0x6c};
static constexpr std::uint8_t PCM_GUID[16] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
static constexpr std::uint8_t TYPE_43_GUID[16] = {0x1c, 0xe0, 0x10, 0x9e, 0xcc, 0x8b, 0xd9, 0x47, 0x8f, 0x3b, 0xe3, 0x0b, 0x8b, 0xdc, 0x35, 0x29};
static constexpr std::size_t FMT_ATRAC9_SIZE = 52;
static constexpr std::size_t FMT_EXTENSIBLE_SIZE = 40;
static constexpr std::size_t FMT_GUID_OFFSET = 24;
static constexpr std::size_t FMT_CONFIG_OFFSET = 44;
static constexpr std::size_t FACT_ATRAC9_SIZE = 12;
static constexpr std::uint32_t MAX_SAMPLE_RATE = 192000;
static constexpr std::uint32_t WAVEFORM_TYPE_PCM_FIRST = 0x10;
static constexpr std::uint32_t WAVEFORM_TYPE_PCM_LAST = 0x1b;
static constexpr std::uint32_t WAVEFORM_TYPE_VAG = 0x1c;
static constexpr std::uint32_t VAG_FRAME_SAMPLES = 28;
static constexpr std::uint32_t ATRAC9_FRAME_SAMPLES[16] = {0, 64, 0, 0, 128, 0, 0, 256, 0, 0, 0, 0, 0, 0, 0, 0};
static constexpr std::uint32_t PCM_BYTES_PER_SAMPLE[] = {1, 1, 2, 2, 3, 3, 4, 4, 4, 4, 8, 8};

static bool IsPcmType(std::uint32_t type) {
    return type >= WAVEFORM_TYPE_PCM_FIRST && type <= WAVEFORM_TYPE_PCM_LAST;
}

static Ngs2WaveformBlock PcmBlock(std::uint32_t type, std::uint32_t channels, std::uint32_t pos, std::uint32_t count) {
    const std::uint64_t stride = static_cast<std::uint64_t>(channels) * PCM_BYTES_PER_SAMPLE[type - WAVEFORM_TYPE_PCM_FIRST];
    const auto offset = static_cast<std::uint32_t>(pos * stride);
    const auto end = (pos + count) * static_cast<std::uint32_t>(stride);
    Ngs2WaveformBlock block{};
    block.data_offset = offset;
    block.data_size = static_cast<std::uint32_t>(end - offset);
    block.num_samples = count;
    return block;
}

static Ngs2WaveformBlock VagBlock(std::uint32_t channels, std::uint32_t pos, std::uint32_t count) {
    const std::uint32_t frameBytes = 16u * channels;
    const std::uint32_t first = pos / VAG_FRAME_SAMPLES;
    const std::uint32_t past = (pos + count + VAG_FRAME_SAMPLES - 1u) / VAG_FRAME_SAMPLES;
    const auto offset = static_cast<std::uint32_t>(static_cast<std::uint64_t>(first) * frameBytes);
    Ngs2WaveformBlock block{};
    block.data_offset = offset;
    block.data_size = static_cast<std::uint32_t>(past * frameBytes - offset);
    block.num_samples = (past - first) * VAG_FRAME_SAMPLES;
    return block;
}

static Ngs2WaveformBlock Atrac9Block(std::uint32_t config, std::uint32_t pos, std::uint32_t count) {
    const std::uint32_t rateIndex = (config >> 20) & 0xfu;
    const std::uint32_t frameSamples = ATRAC9_FRAME_SAMPLES[rateIndex];
    if (frameSamples == 0) throw std::runtime_error("NGS2: ATRAC9 waveform blocks at sampling rate index " + std::to_string(rateIndex) + " are not implemented");
    const std::uint32_t superframeShift = (config >> 3) & 3u;
    const std::uint32_t superframeBytes = (((config >> 5) & 0x7ffu) + 1u) << superframeShift;
    const std::uint32_t superframeSamples = frameSamples << superframeShift;
    const std::uint32_t first = pos / superframeSamples;
    const std::uint32_t lead = frameSamples + pos;
    const std::uint32_t past = (count + lead + superframeSamples - 1u) / superframeSamples;
    const std::uint32_t offset = first * superframeBytes;
    Ngs2WaveformBlock block{};
    block.data_offset = offset;
    block.data_size = past * superframeBytes - offset;
    block.num_skip_samples = lead - first * superframeSamples;
    block.num_samples = count;
    return block;
}

void Ngs2Atrac9DecoderDeleter::operator()(void* handle) const {
    Atrac9ReleaseHandle(handle);
}

static void ConfigBytes(std::uint32_t configData, std::uint8_t* config) {
    for (int i = 0; i < 4; i++) config[i] = static_cast<std::uint8_t>(configData >> (24 - 8 * i));
}

static bool CodecInfo(const std::uint8_t* config, Atrac9CodecInfo& info) {
    std::unique_ptr<void, Ngs2Atrac9DecoderDeleter> decoder(Atrac9GetHandle());
    std::uint8_t copy[4];
    std::memcpy(copy, config, sizeof(copy));
    info = {};
    return decoder != nullptr && Atrac9InitDecoder(decoder.get(), copy) == 0 && Atrac9GetCodecInfo(decoder.get(), &info) == 0 && info.channels > 0 &&
           info.framesInSuperframe > 0 && info.frameSamples > 0 && info.superframeSize % info.framesInSuperframe == 0;
}

static bool MatchingCodecInfo(const Ngs2WaveformFormat& format, Atrac9CodecInfo& info) {
    std::uint8_t config[4];
    ConfigBytes(format.config_data, config);
    return CodecInfo(config, info) && static_cast<std::uint32_t>(info.channels) == format.num_channels && static_cast<std::uint32_t>(info.samplingRate) == format.sample_rate;
}

void Ngs2SetupAtrac9(Ngs2Voice& voice, const Ngs2WaveformFormat& format) {
    Atrac9CodecInfo info{};
    if (!MatchingCodecInfo(format, info)) throw std::invalid_argument("NGS2: the ATRAC9 config does not match the waveform format");
    ConfigBytes(format.config_data, voice.atrac9.config);
    voice.atrac9.frameSamples = static_cast<std::uint32_t>(info.frameSamples);
    voice.atrac9.framesInSuperframe = static_cast<std::uint32_t>(info.framesInSuperframe);
    voice.atrac9.superframeBytes = static_cast<std::uint32_t>(info.superframeSize);
    Ngs2RestartAtrac9(voice);
}

std::size_t Ngs2Atrac9BlockBytes(const Ngs2Voice& voice, const Ngs2WaveformBlock& block) {
    const std::uint64_t samples = static_cast<std::uint64_t>(block.num_skip_samples) + block.num_samples;
    const std::uint64_t superframeSamples = static_cast<std::uint64_t>(voice.atrac9.frameSamples) * voice.atrac9.framesInSuperframe;
    return static_cast<std::size_t>((samples + superframeSamples - 1) / superframeSamples * voice.atrac9.superframeBytes);
}

void Ngs2RestartAtrac9(Ngs2Voice& voice) {
    auto& atrac9 = voice.atrac9;
    atrac9.decoder.reset(Atrac9GetHandle());
    if (atrac9.decoder == nullptr || Atrac9InitDecoder(atrac9.decoder.get(), atrac9.config) != 0) throw std::runtime_error("NGS2: ATRAC9 decoder init failed");
    atrac9.window.clear();
    atrac9.windowCursor = 0;
    atrac9.input.clear();
    atrac9.remainingSamples = 0;
    atrac9.skipSamples = 0;
    atrac9.finishOnDrain = false;
}

static std::size_t BufferedFrames(const Ngs2Voice& voice) {
    return voice.atrac9.window.size() / voice.channels - voice.atrac9.windowCursor;
}

static bool GatherSuperframe(Ngs2Voice& voice) {
    auto& atrac9 = voice.atrac9;
    while (atrac9.input.size() < atrac9.superframeBytes) {
        if (voice.blocks.empty()) {
            if (!voice.acceptsBlocks && atrac9.remainingSamples != 0)
                throw std::invalid_argument("NGS2: the ATRAC9 stream ends before its declared samples");
            return false;
        }
        auto& block = voice.blocks.front();
        if (!block.started) {
            if (block.info.num_samples != 0) {
                if (atrac9.remainingSamples != 0 || !atrac9.input.empty())
                    throw std::invalid_argument("NGS2: a new ATRAC9 waveform interrupts an incomplete stream");
                atrac9.remainingSamples = block.info.num_samples;
                atrac9.skipSamples = block.info.num_skip_samples;
            } else if (atrac9.remainingSamples == 0) {
                throw std::invalid_argument("NGS2: an ATRAC9 data-only block has no waveform to continue");
            }
            block.started = true;
        }
        const auto count = std::min<std::size_t>(atrac9.superframeBytes - atrac9.input.size(), block.info.data_size - block.dataCursor);
        atrac9.input.insert(atrac9.input.end(), block.data + block.dataCursor, block.data + block.dataCursor + count);
        block.dataCursor += count;
        voice.decodedBytes += count;
        if (atrac9.input.size() == atrac9.superframeBytes) return true;
        if (!Ngs2FinishBlock(voice)) return false;
    }
    return true;
}

static bool DecodeSuperframe(Ngs2Voice& voice) {
    if (!GatherSuperframe(voice)) return false;
    auto& atrac9 = voice.atrac9;
    if (atrac9.windowCursor != 0) {
        atrac9.window.erase(atrac9.window.begin(), atrac9.window.begin() + static_cast<std::ptrdiff_t>(atrac9.windowCursor) * voice.channels);
        atrac9.windowCursor = 0;
    }
    const std::size_t frameValues = static_cast<std::size_t>(atrac9.frameSamples) * voice.channels;
    const auto superframeSamples = atrac9.frameSamples * atrac9.framesInSuperframe;
    const auto end = atrac9.window.size();
    atrac9.window.resize(end + static_cast<std::size_t>(superframeSamples) * voice.channels);
    std::size_t consumed = 0;
    for (std::uint32_t frame = 0; frame < atrac9.framesInSuperframe; ++frame) {
        int used = 0;
        const int status = Atrac9DecodeF32(atrac9.decoder.get(), atrac9.input.data() + consumed,
            static_cast<int>(atrac9.superframeBytes - consumed), atrac9.window.data() + end + frame * frameValues, &used, 0);
        if (status != 0 || used <= 0 || static_cast<std::size_t>(used) > atrac9.superframeBytes - consumed)
            throw std::runtime_error("NGS2: ATRAC9 decode failed with " + Ngs2Hex(static_cast<std::uint32_t>(status)));
        consumed += static_cast<std::size_t>(used);
    }
    atrac9.input.clear();
    const auto skipped = std::min(atrac9.skipSamples, superframeSamples);
    const auto samples = std::min(atrac9.remainingSamples, superframeSamples - skipped);
    atrac9.skipSamples -= skipped;
    atrac9.remainingSamples -= samples;
    std::memmove(atrac9.window.data() + end, atrac9.window.data() + end + static_cast<std::size_t>(skipped) * voice.channels,
        static_cast<std::size_t>(samples) * voice.channels * sizeof(float));
    atrac9.window.resize(end + static_cast<std::size_t>(samples) * voice.channels);
    if (atrac9.remainingSamples == 0) {
        atrac9.finishOnDrain = true;
    } else if (voice.blocks.front().dataCursor == voice.blocks.front().info.data_size) {
        if (!Ngs2FinishBlock(voice)) return false;
    }
    return true;
}

static bool EnsureFrames(Ngs2Voice& voice, std::size_t count) {
    auto& atrac9 = voice.atrac9;
    while (voice.state == Ngs2PlayState::Playing && BufferedFrames(voice) < count) {
        if (atrac9.finishOnDrain) {
            if (BufferedFrames(voice) != 0) return false;
            atrac9.finishOnDrain = false;
            if (!Ngs2FinishBlock(voice)) return false;
        }
        if (!DecodeSuperframe(voice)) return false;
    }
    return voice.state == Ngs2PlayState::Playing;
}

static bool AdvanceAtrac9(Ngs2Voice& voice) {
    while ((voice.phase >> 32) != 0 && voice.state == Ngs2PlayState::Playing) {
        if (!EnsureFrames(voice, 1)) return false;
        const auto count = std::min<std::uint64_t>(voice.phase >> 32, BufferedFrames(voice));
        voice.atrac9.windowCursor += static_cast<std::uint32_t>(count);
        voice.decodedSamples += count;
        voice.phase -= count << 32;
        if (BufferedFrames(voice) == 0 && voice.atrac9.finishOnDrain) {
            voice.atrac9.finishOnDrain = false;
            if (!Ngs2FinishBlock(voice)) return false;
        }
    }
    return voice.state == Ngs2PlayState::Playing;
}

void Ngs2ConsumeAtrac9(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t systemRate) {
    constexpr double phaseOne = 4294967296.0;
    const auto initialRevision = voice.waveformRevision;
    const auto step = static_cast<std::uint64_t>(std::llround(voice.sampleRate * static_cast<double>(voice.pitch) / systemRate * phaseOne));
    std::uint32_t written = 0;
    for (; written < grain; ++written) {
        if (!AdvanceAtrac9(voice) || !EnsureFrames(voice, 1)) break;
        const auto revision = voice.waveformRevision;
        const auto fraction = static_cast<float>(voice.phase / phaseOne);
        if (fraction != 0.0f) EnsureFrames(voice, 2);
        if (voice.state != Ngs2PlayState::Playing || voice.waveformRevision != revision) break;
        const auto* current = voice.atrac9.window.data() + static_cast<std::size_t>(voice.atrac9.windowCursor) * voice.channels;
        const auto* next = BufferedFrames(voice) > 1 ? current + voice.channels : current;
        for (std::uint32_t channel = 0; channel < voice.channels; ++channel)
            voice.samples[static_cast<std::size_t>(channel) * grain + written] = std::lerp(current[channel], next[channel], fraction);
        voice.phase += step;
    }
    voice.hasSamples = written != 0;
    if (voice.state != Ngs2PlayState::Playing || voice.waveformRevision != initialRevision) return;
    AdvanceAtrac9(voice);
    if (voice.state != Ngs2PlayState::Playing || voice.waveformRevision != initialRevision) return;
    if (!voice.acceptsBlocks && voice.blocks.empty() && BufferedFrames(voice) == 0 && voice.atrac9.remainingSamples != 0)
        throw std::invalid_argument("NGS2: the ATRAC9 stream ends before its declared samples");
}

static std::uint16_t ReadLe16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0] | data[1] << 8);
}

static std::uint32_t ReadLe32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) | static_cast<std::uint32_t>(data[1]) << 8 | static_cast<std::uint32_t>(data[2]) << 16 | static_cast<std::uint32_t>(data[3]) << 24;
}

struct RiffChunks {
    const std::uint8_t* format = nullptr;
    std::size_t formatSize = 0;
    const std::uint8_t* fact = nullptr;
    std::size_t factSize = 0;
    const std::uint8_t* sampler = nullptr;
    std::size_t samplerSize = 0;
    std::size_t dataOffset = 0;
    std::size_t dataSize = 0;
};

template <class TRead>
static int ReadChunks(std::size_t size, TRead&& read, RiffChunks& chunks) {
    if (size < 12) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    const auto* header = read(0, 12);
    if (std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) return SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
    const std::size_t riffEnd = 8 + static_cast<std::size_t>(ReadLe32(header + 4));
    if (riffEnd < 12) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    const std::size_t end = std::min(size, riffEnd);
    for (std::size_t offset = 12; offset + 8 <= end;) {
        const auto* chunk = read(offset, 8);
        const std::size_t payload = offset + 8;
        const std::size_t chunkSize = ReadLe32(chunk + 4);
        if (chunkSize > riffEnd - payload) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
        const bool data = std::memcmp(chunk, "data", 4) == 0;
        if (data) {
            chunks.dataOffset = payload;
            chunks.dataSize = chunkSize;
            if (chunkSize > end - payload) break;
        }
        if (chunkSize > end - payload) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            chunks.format = read(payload, chunkSize);
            chunks.formatSize = chunkSize;
        } else if (std::memcmp(chunk, "fact", 4) == 0) {
            chunks.fact = read(payload, chunkSize);
            chunks.factSize = chunkSize;
        } else if (std::memcmp(chunk, "smpl", 4) == 0) {
            chunks.sampler = read(payload, chunkSize);
            chunks.samplerSize = chunkSize;
        }
        offset = payload + chunkSize + (chunkSize & 1);
    }
    if (chunks.format == nullptr || chunks.dataOffset == 0 || chunks.dataOffset > std::numeric_limits<std::uint32_t>::max()) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    return SCE_NGS2_OK;
}

static void AddAtrac9Block(Ngs2WaveformInfo& info, std::uint32_t begin, std::uint32_t samples, std::uint32_t repeats) {
    auto block = Atrac9Block(info.format.config_data, begin, samples);
    if (block.data_offset > info.data_size || block.data_size > info.data_size - block.data_offset)
        throw std::runtime_error("NGS2: an ATRAC9 loop block reaching past the data chunk is not implemented");
    block.data_offset += info.data_offset;
    block.num_repeats = repeats;
    info.block[info.num_blocks++] = block;
}

static int ParseAtrac9Loop(const RiffChunks& chunks, Ngs2WaveformInfo& info) {
    if (chunks.sampler == nullptr) return SCE_NGS2_OK;
    if (chunks.samplerSize < 32) throw std::runtime_error("NGS2: a smpl chunk too short for its loop count is not implemented");
    if (ReadLe32(chunks.sampler + 28) == 0) return SCE_NGS2_OK;
    if (chunks.samplerSize < 52) throw std::runtime_error("NGS2: a smpl chunk too short for its first loop is not implemented");
    if (ReadLe32(chunks.sampler + 44) < info.num_delay_samples) throw std::runtime_error("NGS2: an ATRAC9 loop starting inside the decoder delay is not implemented");
    const std::uint32_t begin = ReadLe32(chunks.sampler + 44) - info.num_delay_samples;
    const std::uint32_t end = ReadLe32(chunks.sampler + 48) - info.num_delay_samples + 1u;
    info.loop_begin_position = begin;
    info.loop_end_position = end;
    if (end <= begin) return SCE_NGS2_OK;
    if (end > info.num_samples) throw std::runtime_error("NGS2: a waveform loop ending past the last sample is not implemented");
    info.num_blocks = 0;
    if (begin != 0) AddAtrac9Block(info, 0, begin, 0);
    AddAtrac9Block(info, begin, end - begin, UINT32_MAX);
    if (end != info.num_samples) AddAtrac9Block(info, end, info.num_samples - end, 0);
    return SCE_NGS2_OK;
}

static int ParseAtrac9(const RiffChunks& chunks, Ngs2WaveformInfo& info) {
    const auto* format = chunks.format;
    if (chunks.formatSize < FMT_ATRAC9_SIZE || chunks.fact == nullptr || chunks.factSize < FACT_ATRAC9_SIZE) return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    std::uint8_t config[4];
    std::memcpy(config, format + FMT_CONFIG_OFFSET, sizeof(config));
    Atrac9CodecInfo codec{};
    if (!CodecInfo(config, codec) || ReadLe16(format + 2) != codec.channels || ReadLe32(format + 4) != static_cast<std::uint32_t>(codec.samplingRate) ||
        ReadLe16(format + 12) != codec.superframeSize || ReadLe16(format + 18) != static_cast<std::uint32_t>(codec.frameSamples * codec.framesInSuperframe)) {
        return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    }
    info.format = {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, static_cast<std::uint32_t>(codec.channels), static_cast<std::uint32_t>(codec.samplingRate),
                   static_cast<std::uint32_t>(config[0]) << 24 | static_cast<std::uint32_t>(config[1]) << 16 | static_cast<std::uint32_t>(config[2]) << 8 | config[3], 0, 0};
    info.data_offset = static_cast<std::uint32_t>(chunks.dataOffset);
    info.data_size = static_cast<std::uint32_t>(chunks.dataSize);
    info.num_samples = ReadLe32(chunks.fact);
    info.audio_unit_size = static_cast<std::uint32_t>(codec.superframeSize / codec.framesInSuperframe);
    info.num_audio_unit_samples = static_cast<std::uint32_t>(codec.frameSamples);
    info.num_audio_unit_per_frame = static_cast<std::uint32_t>(codec.framesInSuperframe);
    info.audio_frame_size = static_cast<std::uint32_t>(codec.superframeSize);
    info.num_audio_frame_samples = static_cast<std::uint32_t>(codec.frameSamples * codec.framesInSuperframe);
    info.num_delay_samples = static_cast<std::uint32_t>(codec.frameSamples);
    info.num_blocks = 1;
    info.block[0].data_offset = chunks.dataOffset;
    info.block[0].data_size = chunks.dataSize;
    info.block[0].num_skip_samples = ReadLe32(chunks.fact + 8);
    info.block[0].num_samples = info.num_samples;
    return ParseAtrac9Loop(chunks, info);
}

static std::uint32_t PcmWaveformType(std::uint32_t bits, bool floating) {
    if (floating) return bits == 32 ? 0x18 : bits == 64 ? 0x1a : 0;
    return bits == 8 ? 0x11 : bits == 16 ? SCE_NGS2_WAVEFORM_TYPE_PCM_I16L : bits == 24 ? 0x14 : bits == 32 ? 0x16 : 0;
}

static int ParsePcm(const RiffChunks& chunks, std::size_t size, Ngs2WaveformInfo& info, bool floating) {
    const auto* format = chunks.format;
    if (chunks.formatSize < 16) return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    const std::uint32_t bits = ReadLe16(format + 14);
    const std::uint32_t type = PcmWaveformType(bits, floating);
    if (type == 0) return SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
    const std::uint32_t channels = ReadLe16(format + 2);
    const std::uint32_t sampleRate = ReadLe32(format + 4);
    const std::uint32_t bytesPerFrame = channels * (bits / 8);
    if (channels == 0 || channels > NGS2_MAX_CHANNELS || sampleRate == 0 || sampleRate > MAX_SAMPLE_RATE || ReadLe16(format + 12) != bytesPerFrame) {
        return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    }
    if (chunks.sampler != nullptr && chunks.samplerSize >= 32 && ReadLe32(chunks.sampler + 28) != 0) throw std::runtime_error("NGS2: parsing looped waveforms is not implemented");
    if (chunks.dataSize > size - chunks.dataOffset || chunks.dataSize % bytesPerFrame != 0) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    const std::size_t dataSize = chunks.dataSize;
    info.format = {type, channels, sampleRate, 0, 0, 0};
    info.data_offset = static_cast<std::uint32_t>(chunks.dataOffset);
    info.data_size = static_cast<std::uint32_t>(dataSize);
    info.num_samples = static_cast<std::uint32_t>(dataSize / bytesPerFrame);
    info.audio_unit_size = bytesPerFrame;
    info.num_audio_unit_samples = 1;
    info.num_audio_unit_per_frame = 1;
    info.audio_frame_size = bytesPerFrame;
    info.num_audio_frame_samples = 1;
    info.num_blocks = 1;
    info.block[0].data_offset = chunks.dataOffset;
    info.block[0].data_size = dataSize;
    info.block[0].num_samples = info.num_samples;
    return SCE_NGS2_OK;
}

static int ParseWaveform(const RiffChunks& chunks, std::size_t size, Ngs2WaveformInfo& info) {
    if (chunks.formatSize < 2) return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    const std::uint16_t tag = ReadLe16(chunks.format);
    if (tag == WAVE_FORMAT_EXTENSIBLE) {
        if (chunks.formatSize < FMT_EXTENSIBLE_SIZE) return SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
        const auto* guid = chunks.format + FMT_GUID_OFFSET;
        if (std::memcmp(guid, PCM_GUID, sizeof(PCM_GUID)) == 0) return ParsePcm(chunks, size, info, false);
        if (std::memcmp(guid, ATRAC9_GUID, sizeof(ATRAC9_GUID)) == 0) return ParseAtrac9(chunks, info);
        if (chunks.formatSize == FMT_ATRAC9_SIZE) throw std::runtime_error("NGS2: parsing an extensible waveform of format size 0x34 without the ATRAC9 GUID is not implemented");
        if (std::memcmp(guid, TYPE_43_GUID, sizeof(TYPE_43_GUID)) == 0) throw std::runtime_error("NGS2: parsing waveform type 0x43 is not implemented");
        return SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
    }
    if (tag == WAVE_FORMAT_PCM) return ParsePcm(chunks, size, info, false);
    if (tag == WAVE_FORMAT_IEEE_FLOAT) return ParsePcm(chunks, size, info, true);
    return SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceNgs2ParseWaveformData(const void* data, size_t data_size, Ngs2WaveformInfo* info) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *info = {};
    if (data == nullptr) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    RiffChunks chunks;
    const int result = ReadChunks(data_size, [bytes](std::size_t at, std::size_t) { return bytes + at; }, chunks);
    if (result != SCE_NGS2_OK) return result;
    return ParseWaveform(chunks, data_size, *info);
}

int APS5_VABI sceNgs2ParseWaveformFile(const char* path, uint32_t offset, Ngs2WaveformInfo* info) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *info = {};
    if (path == nullptr) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    const auto host = ResolvePath_nid_no_patch(path);
    std::ifstream file(host, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error(std::string(__func__) + ": cannot open " + host.string());
    const auto fileSize = static_cast<std::uint64_t>(file.tellg());
    if (offset > fileSize) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    const auto length = static_cast<std::size_t>(fileSize - offset);
    const std::string function = __func__;
    std::vector<std::vector<std::uint8_t>> pieces;
    const auto readBytes = [&](std::size_t at, std::size_t count) {
        auto& piece = pieces.emplace_back(std::max<std::size_t>(count, 1));
        file.seekg(static_cast<std::streamoff>(offset + at));
        if (!file.read(reinterpret_cast<char*>(piece.data()), static_cast<std::streamsize>(count))) throw std::runtime_error(function + ": read failed for " + host.string());
        return static_cast<const std::uint8_t*>(piece.data());
    };
    RiffChunks chunks;
    int result = ReadChunks(length, readBytes, chunks);
    if (result == SCE_NGS2_OK) result = ParseWaveform(chunks, length, *info);
    if (result != SCE_NGS2_OK) return result;
    info->data_offset += offset;
    for (std::uint32_t i = 0; i < info->num_blocks; i++) info->block[i].data_offset += offset;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2CalcWaveformBlock(const Ngs2WaveformFormat* format, uint32_t sample_pos, uint32_t num_samples, Ngs2WaveformBlock* block) {
    if (block == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *block = {};
    if (format == nullptr) return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    const auto type = format->waveform_type;
    if (IsPcmType(type)) {
        *block = PcmBlock(type, format->num_channels, sample_pos, num_samples);
        return SCE_NGS2_OK;
    }
    if (type == WAVEFORM_TYPE_VAG) {
        *block = VagBlock(format->num_channels, sample_pos, num_samples);
        return SCE_NGS2_OK;
    }
    if (type != SCE_NGS2_WAVEFORM_TYPE_ATRAC9) return SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
    *block = Atrac9Block(format->config_data, sample_pos, num_samples);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2GetWaveformFrameInfo(const Ngs2WaveformFormat* format, uint32_t* out_frame_size, uint32_t* out_num_frame_samples, uint32_t* out_units_per_frame, uint32_t* out_num_delay_samples) {
    if (out_frame_size == nullptr || out_num_frame_samples == nullptr || out_units_per_frame == nullptr || out_num_delay_samples == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (format == nullptr || format->num_channels == 0 || format->num_channels > NGS2_MAX_CHANNELS || format->sample_rate == 0 || format->sample_rate > MAX_SAMPLE_RATE) {
        return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    }
    if (format->waveform_type == SCE_NGS2_WAVEFORM_TYPE_PCM_I16L) {
        *out_frame_size = format->num_channels * sizeof(std::int16_t);
        *out_num_frame_samples = 1;
        *out_units_per_frame = 1;
        *out_num_delay_samples = 0;
        return SCE_NGS2_OK;
    }
    if (format->waveform_type != SCE_NGS2_WAVEFORM_TYPE_ATRAC9) throw std::runtime_error("NGS2: the frame info of waveform type " + Ngs2Hex(format->waveform_type) + " is not implemented");
    Atrac9CodecInfo codec{};
    if (!MatchingCodecInfo(*format, codec)) return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    *out_frame_size = static_cast<std::uint32_t>(codec.superframeSize);
    *out_num_frame_samples = static_cast<std::uint32_t>(codec.frameSamples * codec.framesInSuperframe);
    *out_units_per_frame = static_cast<std::uint32_t>(codec.framesInSuperframe);
    *out_num_delay_samples = static_cast<std::uint32_t>(codec.frameSamples);
    return SCE_NGS2_OK;
}

}

#pragma GCC visibility pop
