#include "Ngs2Test.hpp"

#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <vector>

static void Put16(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

static void Put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    Put16(out, value & 0xffff);
    Put16(out, value >> 16);
}

static void PutTag(std::vector<std::uint8_t>& out, const char* tag) {
    out.insert(out.end(), tag, tag + 4);
}

static std::vector<std::uint8_t> PcmFile(std::uint32_t channels, std::uint32_t sampleRate, std::uint32_t bits, std::uint32_t frames, std::uint32_t tag = 1) {
    const std::uint32_t frameBytes = channels * bits / 8;
    const std::uint32_t formatSize = tag == 0xfffe ? 40 : 16;
    std::vector<std::uint8_t> file;
    PutTag(file, "RIFF");
    Put32(file, 20 + formatSize + frames * frameBytes);
    PutTag(file, "WAVE");
    PutTag(file, "fmt ");
    Put32(file, formatSize);
    Put16(file, tag);
    Put16(file, channels);
    Put32(file, sampleRate);
    Put32(file, sampleRate * frameBytes);
    Put16(file, frameBytes);
    Put16(file, bits);
    if (tag == 0xfffe) {
        Put16(file, 22);
        Put16(file, bits);
        Put32(file, 0);
        const std::uint8_t pcm[16] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
        file.insert(file.end(), pcm, pcm + sizeof(pcm));
    }
    PutTag(file, "data");
    Put32(file, frames * frameBytes);
    file.resize(file.size() + frames * frameBytes, 0x11);
    return file;
}

static void TestParsePcm() {
    const auto file = PcmFile(2, 44100, 16, 100);
    Ngs2WaveformInfo info{};
    Require(sceNgs2ParseWaveformData(file.data(), file.size(), &info) == SCE_NGS2_OK);
    Require(info.format.waveform_type == SCE_NGS2_WAVEFORM_TYPE_PCM_I16L && info.format.num_channels == 2 && info.format.sample_rate == 44100);
    Require(info.data_offset == 44 && info.data_size == 400 && info.num_samples == 100);
    Require(info.num_blocks == 1 && info.block[0].data_offset == 44 && info.block[0].data_size == 400 && info.block[0].num_samples == 100);

    auto truncated = file;
    truncated.resize(44 + 202);
    Require(sceNgs2ParseWaveformData(truncated.data(), truncated.size(), &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);

    auto partialFrame = PcmFile(2, 44100, 16, 100);
    partialFrame.resize(44 + 402, 0x11);
    const std::uint32_t partialSize = 402;
    std::memcpy(partialFrame.data() + 40, &partialSize, sizeof(partialSize));
    Require(sceNgs2ParseWaveformData(partialFrame.data(), partialFrame.size(), &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);

    struct Depth { std::uint32_t tag, bits, type; };
    for (const auto depth : {Depth{1, 8, 0x11}, Depth{1, 16, 0x12}, Depth{1, 24, 0x14}, Depth{1, 32, 0x16}, Depth{3, 32, 0x18}, Depth{3, 64, 0x1a},
                             Depth{0xfffe, 8, 0x11}, Depth{0xfffe, 24, 0x14}}) {
        const auto wave = PcmFile(3, 48000, depth.bits, 10, depth.tag);
        const std::uint32_t frameBytes = 3 * depth.bits / 8;
        const std::uint32_t dataOffset = static_cast<std::uint32_t>(wave.size() - 10 * frameBytes);
        Ngs2WaveformInfo parsed{};
        Require(sceNgs2ParseWaveformData(wave.data(), wave.size(), &parsed) == SCE_NGS2_OK);
        Require(parsed.format.waveform_type == depth.type && parsed.format.num_channels == 3 && parsed.format.sample_rate == 48000);
        Require(parsed.data_offset == dataOffset && parsed.data_size == 10 * frameBytes && parsed.num_samples == 10);
        Require(parsed.audio_unit_size == frameBytes && parsed.num_audio_unit_samples == 1 && parsed.audio_frame_size == frameBytes && parsed.num_audio_frame_samples == 1);
        Require(parsed.num_blocks == 1 && parsed.block[0].data_offset == dataOffset && parsed.block[0].data_size == 10 * frameBytes && parsed.block[0].num_samples == 10);
    }
    for (const auto depth : {Depth{1, 12, 0}, Depth{1, 64, 0}, Depth{3, 16, 0}, Depth{3, 24, 0}, Depth{0xfffe, 12, 0}}) {
        const auto wave = PcmFile(1, 44100, depth.bits, 8, depth.tag);
        Ngs2WaveformInfo parsed{};
        Require(sceNgs2ParseWaveformData(wave.data(), wave.size(), &parsed) == SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);
    }
    const auto noChannels = PcmFile(0, 44100, 16, 10);
    Require(sceNgs2ParseWaveformData(noChannels.data(), noChannels.size(), &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT);
}

static void AppendLoops(std::vector<std::uint8_t>& file, std::uint32_t loops, std::uint32_t type, std::uint32_t begin, std::uint32_t lastSample, std::uint32_t plays,
                        std::uint32_t fraction = 0) {
    PutTag(file, "smpl");
    Put32(file, 36 + 24 * loops);
    for (int i = 0; i < 7; ++i) Put32(file, 0);
    Put32(file, loops);
    Put32(file, 0);
    for (std::uint32_t loop = 0; loop < loops; ++loop) {
        Put32(file, loop);
        Put32(file, type);
        Put32(file, begin);
        Put32(file, lastSample);
        Put32(file, fraction);
        Put32(file, plays);
    }
    const auto riffSize = static_cast<std::uint32_t>(file.size() - 8);
    std::memcpy(file.data() + 4, &riffSize, sizeof(riffSize));
}

static bool ParseThrows(const std::vector<std::uint8_t>& file) {
    Ngs2WaveformInfo info{};
    try {
        sceNgs2ParseWaveformData(file.data(), file.size(), &info);
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static void RequireBlock(const Ngs2WaveformBlock& block, std::uint64_t offset, std::uint64_t size, std::uint32_t repeats, std::uint32_t samples) {
    Require(block.data_offset == offset && block.data_size == size && block.num_repeats == repeats && block.num_skip_samples == 0 && block.num_samples == samples);
}

static void AppendShortSampler(std::vector<std::uint8_t>& file, std::uint32_t size, std::uint32_t loops) {
    PutTag(file, "smpl");
    Put32(file, size);
    for (std::uint32_t offset = 0; offset < size; offset += 4) Put32(file, offset == 28 ? loops : 0);
    const auto riffSize = static_cast<std::uint32_t>(file.size() - 8);
    std::memcpy(file.data() + 4, &riffSize, sizeof(riffSize));
}

static Ngs2WaveformInfo ParseLooped(std::uint32_t loops, std::uint32_t type, std::uint32_t begin, std::uint32_t lastSample, std::uint32_t plays, std::uint32_t fraction = 0) {
    auto file = PcmFile(1, 44100, 16, 100);
    AppendLoops(file, loops, type, begin, lastSample, plays, fraction);
    Ngs2WaveformInfo info{};
    Require(sceNgs2ParseWaveformData(file.data(), file.size(), &info) == SCE_NGS2_OK);
    return info;
}

static void TestParsePcmLoops() {
    for (const std::uint32_t bits : {16u, 24u}) {
        const std::uint32_t frame = 2 * bits / 8;
        auto looped = PcmFile(2, 48000, bits, 100);
        AppendLoops(looped, 1, 0, 10, 59, 0);
        Ngs2WaveformInfo info{};
        Require(sceNgs2ParseWaveformData(looped.data(), looped.size(), &info) == SCE_NGS2_OK);
        Require(info.num_samples == 100 && info.loop_begin_position == 10 && info.loop_end_position == 60 && info.num_blocks == 3);
        RequireBlock(info.block[0], 44, 10 * frame, 0, 10);
        RequireBlock(info.block[1], 44 + 10 * frame, 50 * frame, UINT32_MAX, 50);
        RequireBlock(info.block[2], 44 + 60 * frame, 40 * frame, 0, 40);
    }

    for (const auto& info : {ParseLooped(1, 0, 10, 59, 3), ParseLooped(2, 1, 10, 59, 0), ParseLooped(3, 2, 10, 59, 7, 0x80000000u)}) {
        Require(info.loop_begin_position == 10 && info.loop_end_position == 60 && info.num_blocks == 3);
        RequireBlock(info.block[0], 44, 20, 0, 10);
        RequireBlock(info.block[1], 64, 100, UINT32_MAX, 50);
        RequireBlock(info.block[2], 164, 80, 0, 40);
    }

    auto info = ParseLooped(1, 0, 0, 99, 3);
    Require(info.loop_begin_position == 0 && info.loop_end_position == 100 && info.num_blocks == 1);
    RequireBlock(info.block[0], 44, 200, UINT32_MAX, 100);

    info = ParseLooped(1, 0, 0, 49, 0);
    Require(info.loop_begin_position == 0 && info.loop_end_position == 50 && info.num_blocks == 2);
    RequireBlock(info.block[0], 44, 100, UINT32_MAX, 50);
    RequireBlock(info.block[1], 144, 100, 0, 50);

    info = ParseLooped(1, 0, 30, 99, 0);
    Require(info.loop_begin_position == 30 && info.loop_end_position == 100 && info.num_blocks == 2);
    RequireBlock(info.block[0], 44, 60, 0, 30);
    RequireBlock(info.block[1], 104, 140, UINT32_MAX, 70);

    info = ParseLooped(1, 0, 50, 50, 0);
    Require(info.loop_begin_position == 50 && info.loop_end_position == 51 && info.num_blocks == 3);
    RequireBlock(info.block[1], 144, 2, UINT32_MAX, 1);
    RequireBlock(info.block[2], 146, 98, 0, 49);

    for (const auto [begin, lastSample, end] : {std::tuple{50u, 49u, 50u}, {50u, 10u, 11u}, {20u, 0xffffffffu, 0u}}) {
        info = ParseLooped(1, 0, begin, lastSample, 0);
        Require(info.loop_begin_position == begin && info.loop_end_position == end && info.num_blocks == 1);
        RequireBlock(info.block[0], 44, 200, 0, 100);
    }

    info = ParseLooped(0, 0, 10, 59, 0);
    Require(info.num_blocks == 1 && info.loop_begin_position == 0 && info.loop_end_position == 0);
    RequireBlock(info.block[0], 44, 200, 0, 100);

    for (const auto [begin, lastSample] : {std::pair{0u, 100u}, {100u, 120u}}) {
        auto pastEnd = PcmFile(1, 44100, 16, 100);
        AppendLoops(pastEnd, 1, 0, begin, lastSample, 0);
        Require(ParseThrows(pastEnd));
    }
    for (const auto [size, loops] : {std::pair{28u, 0u}, {48u, 1u}}) {
        auto shortSampler = PcmFile(1, 44100, 16, 100);
        AppendShortSampler(shortSampler, size, loops);
        Require(ParseThrows(shortSampler));
    }
    auto emptySampler = PcmFile(1, 44100, 16, 100);
    AppendShortSampler(emptySampler, 36, 0);
    Require(sceNgs2ParseWaveformData(emptySampler.data(), emptySampler.size(), &info) == SCE_NGS2_OK && info.num_blocks == 1);
}

static void TestParseFile() {
    const auto wave = PcmFile(1, 22050, 16, 64);
    const char* const guestPath = "/aps5_ngs2_parse_file.wav";
    const auto path = ResolvePath_nid_no_patch(guestPath);
    {
        std::ofstream out(path, std::ios::binary);
        const char pad[3]{};
        out.write(pad, sizeof(pad));
        out.write(reinterpret_cast<const char*>(wave.data()), static_cast<std::streamsize>(wave.size()));
    }
    Ngs2WaveformInfo info{};
    Require(sceNgs2ParseWaveformFile(guestPath, 3, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2ParseWaveformFile(nullptr, 0, &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);
    Require(sceNgs2ParseWaveformFile(guestPath, 3, &info) == SCE_NGS2_OK);
    Require(info.format.num_channels == 1 && info.format.sample_rate == 22050 && info.num_samples == 64);
    Require(info.data_offset == 3 + 44 && info.block[0].data_offset == 3 + 44 && info.data_size == 128);
    Require(sceNgs2ParseWaveformFile(guestPath, static_cast<std::uint32_t>(wave.size()) + 4, &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);
    Require(sceNgs2ParseWaveformFile(guestPath, 0, &info) == SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);
    std::filesystem::remove(path);
}

static void TestParseFileBank() {
    const auto first = PcmFile(2, 48000, 16, 300);
    auto second = PcmFile(1, 32000, 16, 50);
    PutTag(second, "LIST");
    Put32(second, 4);
    PutTag(second, "INFO");
    const std::uint32_t riffSize = static_cast<std::uint32_t>(second.size() - 8);
    std::memcpy(second.data() + 4, &riffSize, sizeof(riffSize));
    const char* const guestPath = "/aps5_ngs2_parse_bank.bin";
    const auto path = ResolvePath_nid_no_patch(guestPath);
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(first.data()), static_cast<std::streamsize>(first.size()));
        out.write(reinterpret_cast<const char*>(second.data()), static_cast<std::streamsize>(second.size()));
    }
    Ngs2WaveformInfo info{};
    Require(sceNgs2ParseWaveformFile(guestPath, 0, &info) == SCE_NGS2_OK);
    Require(info.format.num_channels == 2 && info.format.sample_rate == 48000 && info.num_samples == 300);
    Require(info.data_offset == 44 && info.data_size == 1200 && info.block[0].data_offset == 44);
    const auto secondOffset = static_cast<std::uint32_t>(first.size());
    Require(sceNgs2ParseWaveformFile(guestPath, secondOffset, &info) == SCE_NGS2_OK);
    Require(info.format.num_channels == 1 && info.format.sample_rate == 32000 && info.num_samples == 50);
    Require(info.data_offset == secondOffset + 44 && info.data_size == 100 && info.block[0].data_offset == secondOffset + 44);
    std::filesystem::remove(path);
}

static void TestFrameInfoPcm() {
    for (std::uint32_t channels = 1; channels <= 8; channels++) {
        for (const std::uint32_t rate : {1u, 22050u, 48000u, 192000u}) {
            const Ngs2WaveformFormat format{SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, channels, rate, 0, 0, 0};
            std::uint32_t frameSize = 0xa1, frameSamples = 0xa2, units = 0xa3, delay = 0xa4;
            Require(sceNgs2GetWaveformFrameInfo(&format, &frameSize, &frameSamples, &units, &delay) == SCE_NGS2_OK);
            Require(frameSize == channels * 2 && frameSamples == 1 && units == 1 && delay == 0);
            const auto file = PcmFile(channels, rate, 16, 10);
            Ngs2WaveformInfo info{};
            Require(sceNgs2ParseWaveformData(file.data(), file.size(), &info) == SCE_NGS2_OK);
            Require(info.audio_frame_size == frameSize && info.num_audio_frame_samples == frameSamples);
            Require(info.num_audio_unit_per_frame == units && info.num_delay_samples == delay);
        }
    }
    const Ngs2WaveformFormat ignored{SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0x12345678, 3, 5};
    std::uint32_t frameSize = 0xa1, frameSamples = 0xa2, units = 0xa3, delay = 0xa4;
    Require(sceNgs2GetWaveformFrameInfo(&ignored, &frameSize, &frameSamples, &units, &delay) == SCE_NGS2_OK);
    Require(frameSize == 4 && frameSamples == 1 && units == 1 && delay == 0);
}

static void TestFrameInfoErrors() {
    const Ngs2WaveformFormat pcm{SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0};
    std::uint32_t values[4] = {0xa1, 0xa2, 0xa3, 0xa4};
    const auto untouched = [&] { return values[0] == 0xa1 && values[1] == 0xa2 && values[2] == 0xa3 && values[3] == 0xa4; };
    Require(sceNgs2GetWaveformFrameInfo(&pcm, nullptr, &values[1], &values[2], &values[3]) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2GetWaveformFrameInfo(&pcm, &values[0], nullptr, &values[2], &values[3]) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2GetWaveformFrameInfo(&pcm, &values[0], &values[1], nullptr, &values[3]) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2GetWaveformFrameInfo(&pcm, &values[0], &values[1], &values[2], nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2GetWaveformFrameInfo(nullptr, nullptr, nullptr, nullptr, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2GetWaveformFrameInfo(nullptr, &values[0], &values[1], &values[2], &values[3]) == SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT);
    Require(untouched());
    const Ngs2WaveformFormat invalid[] = {
        {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 0, 48000, 0, 0, 0},
        {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 9, 48000, 0, 0, 0},
        {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 0, 0, 0, 0},
        {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 192001, 0, 0, 0},
        {0x99, 0, 48000, 0, 0, 0},
        {SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2, 0, 0, 0, 0},
    };
    for (const auto& format : invalid) {
        Require(sceNgs2GetWaveformFrameInfo(&format, &values[0], &values[1], &values[2], &values[3]) == SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT);
        Require(untouched());
    }
    for (const std::uint32_t type : {0u, 0x10u, 0x11u, 0x13u, 0x14u, 0x16u, SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 0x19u, 0x41u, 0xffffffffu}) {
        Require(FrameInfoThrows({type, 2, 48000, 0, 0, 0}));
    }
}

int main() {
    TestParsePcm();
    TestParsePcmLoops();
    TestParseFile();
    TestParseFileBank();
    TestFrameInfoPcm();
    TestFrameInfoErrors();
    return 0;
}
