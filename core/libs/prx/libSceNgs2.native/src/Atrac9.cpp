#include <algorithm>
#include <cstddef>
#include <cstdint>
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
static constexpr std::uint16_t WAVE_FORMAT_EXTENSIBLE = 0xfffe;
static constexpr std::uint8_t ATRAC9_GUID[16] = {0xd2, 0x42, 0xe1, 0x47, 0xba, 0x36, 0x8d, 0x4d, 0x88, 0xfc, 0x61, 0x65, 0x4f, 0x8c, 0x83, 0x6c};
static constexpr std::size_t FMT_ATRAC9_SIZE = 52;
static constexpr std::size_t FMT_GUID_OFFSET = 24;
static constexpr std::size_t FMT_CONFIG_OFFSET = 44;
static constexpr std::size_t FACT_ATRAC9_SIZE = 12;
static constexpr std::uint32_t MAX_SAMPLE_RATE = 192000;

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
    atrac9.windowStart = 0;
}

static void DecodeSuperframe(Ngs2Voice& voice, Ngs2Block& block) {
    auto& atrac9 = voice.atrac9;
    if (block.dataCursor + atrac9.superframeBytes > block.info.data_size) throw std::invalid_argument("NGS2: the ATRAC9 block ends inside a superframe");
    const std::size_t frameValues = static_cast<std::size_t>(atrac9.frameSamples) * voice.channels;
    const std::size_t superframeValues = frameValues * atrac9.framesInSuperframe;
    if (atrac9.window.size() >= 2 * superframeValues) {
        atrac9.window.erase(atrac9.window.begin(), atrac9.window.begin() + static_cast<std::ptrdiff_t>(superframeValues));
        atrac9.windowStart += atrac9.frameSamples * atrac9.framesInSuperframe;
    }
    std::size_t end = atrac9.window.size();
    atrac9.window.resize(end + superframeValues);
    const auto* superframe = block.data + block.dataCursor;
    std::size_t consumed = 0;
    for (std::uint32_t frame = 0; frame < atrac9.framesInSuperframe; frame++, end += frameValues) {
        int used = 0;
        const int status = Atrac9DecodeF32(atrac9.decoder.get(), superframe + consumed, static_cast<int>(atrac9.superframeBytes - consumed), atrac9.window.data() + end, &used, 0);
        consumed += static_cast<std::size_t>(std::max(used, 0));
        if (status != 0 || consumed > atrac9.superframeBytes) throw std::runtime_error("NGS2: ATRAC9 decode failed with " + Ngs2Hex(static_cast<std::uint32_t>(status)));
    }
    block.dataCursor += atrac9.superframeBytes;
    voice.decodedBytes += atrac9.superframeBytes;
}

const float* Ngs2Atrac9Frame(Ngs2Voice& voice, Ngs2Block& block, std::uint32_t frame) {
    auto& atrac9 = voice.atrac9;
    const std::uint32_t position = block.info.num_skip_samples + frame;
    if (block.dataCursor == 0) {
        atrac9.window.clear();
        atrac9.windowStart = 0;
    }
    if (position < atrac9.windowStart) throw std::logic_error("NGS2: ATRAC9 frames are read backwards");
    while (position >= atrac9.windowStart + atrac9.window.size() / voice.channels) DecodeSuperframe(voice, block);
    return atrac9.window.data() + static_cast<std::size_t>(position - atrac9.windowStart) * voice.channels;
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

static int ReadChunks(const std::uint8_t* bytes, std::size_t size, RiffChunks& chunks) {
    if (size < 12) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    if (std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "WAVE", 4) != 0) return SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT;
    const std::size_t end = std::min<std::size_t>(size, 8 + static_cast<std::size_t>(ReadLe32(bytes + 4)));
    for (std::size_t offset = 12; offset + 8 <= end;) {
        const auto* chunk = bytes + offset;
        const std::size_t payload = offset + 8;
        const std::size_t chunkSize = ReadLe32(chunk + 4);
        const bool data = std::memcmp(chunk, "data", 4) == 0;
        if (data) {
            chunks.dataOffset = payload;
            chunks.dataSize = chunkSize;
            break;
        }
        if (chunkSize > end - payload) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            chunks.format = bytes + payload;
            chunks.formatSize = chunkSize;
        } else if (std::memcmp(chunk, "fact", 4) == 0) {
            chunks.fact = bytes + payload;
            chunks.factSize = chunkSize;
        } else if (std::memcmp(chunk, "smpl", 4) == 0) {
            chunks.sampler = bytes + payload;
            chunks.samplerSize = chunkSize;
        }
        offset = payload + chunkSize + (chunkSize & 1);
    }
    if (chunks.format == nullptr || chunks.dataOffset == 0 || chunks.dataOffset > std::numeric_limits<std::uint32_t>::max()) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
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
    if (chunks.sampler != nullptr && chunks.samplerSize >= 32 && ReadLe32(chunks.sampler + 28) != 0) throw std::runtime_error("NGS2: parsing looped waveforms is not implemented");
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
    info.num_delay_samples = ReadLe32(chunks.fact + 4);
    info.num_blocks = 1;
    info.block[0].data_offset = chunks.dataOffset;
    info.block[0].data_size = chunks.dataSize;
    info.block[0].num_skip_samples = ReadLe32(chunks.fact + 8);
    info.block[0].num_samples = info.num_samples;
    return SCE_NGS2_OK;
}

static int ParsePcm16(const RiffChunks& chunks, std::size_t size, Ngs2WaveformInfo& info) {
    const auto* format = chunks.format;
    if (chunks.formatSize < 16) return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    const std::uint32_t channels = ReadLe16(format + 2);
    const std::uint32_t sampleRate = ReadLe32(format + 4);
    const std::uint32_t bytesPerFrame = channels * sizeof(std::int16_t);
    if (ReadLe16(format + 14) != 16 || channels == 0 || channels > NGS2_MAX_CHANNELS || sampleRate == 0 || sampleRate > MAX_SAMPLE_RATE || ReadLe16(format + 12) != bytesPerFrame) {
        return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    }
    if (chunks.sampler != nullptr && chunks.samplerSize >= 32 && ReadLe32(chunks.sampler + 28) != 0) throw std::runtime_error("NGS2: parsing looped waveforms is not implemented");
    if (chunks.dataSize > size - chunks.dataOffset || chunks.dataSize % bytesPerFrame != 0) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    const std::size_t dataSize = chunks.dataSize;
    info.format = {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, channels, sampleRate, 0, 0, 0};
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

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceNgs2ParseWaveformData(const void* data, size_t data_size, Ngs2WaveformInfo* info) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *info = {};
    if (data == nullptr) return SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    RiffChunks chunks;
    const int result = ReadChunks(static_cast<const std::uint8_t*>(data), data_size, chunks);
    if (result != SCE_NGS2_OK) return result;
    const std::uint16_t tag = ReadLe16(chunks.format);
    if (tag == WAVE_FORMAT_EXTENSIBLE && chunks.formatSize >= FMT_GUID_OFFSET + sizeof(ATRAC9_GUID) &&
        std::memcmp(chunks.format + FMT_GUID_OFFSET, ATRAC9_GUID, sizeof(ATRAC9_GUID)) == 0) {
        return ParseAtrac9(chunks, *info);
    }
    if (tag == WAVE_FORMAT_PCM) return ParsePcm16(chunks, data_size, *info);
    throw std::runtime_error("NGS2: parsing waveform format tag " + Ngs2Hex(tag) + " is not implemented");
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
    const std::uint64_t length = fileSize - offset;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    file.seekg(offset);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error(std::string(__func__) + ": read failed for " + host.string());
    const int result = sceNgs2ParseWaveformData(bytes.data(), bytes.size(), info);
    if (result != SCE_NGS2_OK) return result;
    info->data_offset += offset;
    for (std::uint32_t i = 0; i < info->num_blocks; i++) info->block[i].data_offset += offset;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2CalcWaveformBlock(const Ngs2WaveformFormat* format, uint32_t sample_pos, uint32_t num_samples, Ngs2WaveformBlock* block) {
    if (block == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *block = {};
    if (format == nullptr || format->num_channels == 0 || format->num_channels > NGS2_MAX_CHANNELS || format->sample_rate == 0 || format->sample_rate > MAX_SAMPLE_RATE) {
        return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    }
    if (format->waveform_type != SCE_NGS2_WAVEFORM_TYPE_ATRAC9) throw std::runtime_error("NGS2: waveform blocks of type " + Ngs2Hex(format->waveform_type) + " are not implemented");
    Atrac9CodecInfo codec{};
    if (!MatchingCodecInfo(*format, codec)) return SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT;
    const std::uint64_t superframeSamples = static_cast<std::uint64_t>(codec.frameSamples) * codec.framesInSuperframe;
    const std::uint64_t skip = sample_pos % superframeSamples;
    block->data_offset = sample_pos / superframeSamples * codec.superframeSize;
    if (num_samples != 0) {
        block->data_size = (skip + num_samples + superframeSamples - 1) / superframeSamples * codec.superframeSize;
        block->num_skip_samples = static_cast<std::uint32_t>(skip);
    }
    block->num_samples = num_samples;
    return SCE_NGS2_OK;
}

}

#pragma GCC visibility pop
