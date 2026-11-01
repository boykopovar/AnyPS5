#include "Ngs2Test.hpp"

#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
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

static std::vector<std::uint8_t> ExtensibleFile(std::uint32_t formatSize, const std::uint8_t* guid) {
    const auto pcm = PcmFile(1, 44100, 16, 10, 0xfffe);
    std::vector<std::uint8_t> format(pcm.begin() + 20, pcm.begin() + 60);
    std::memcpy(format.data() + 24, guid, 16);
    format.resize(formatSize);
    std::vector<std::uint8_t> file;
    PutTag(file, "RIFF");
    Put32(file, 0);
    PutTag(file, "WAVE");
    PutTag(file, "fmt ");
    Put32(file, formatSize);
    file.insert(file.end(), format.begin(), format.end());
    file.insert(file.end(), pcm.begin() + 60, pcm.end());
    const auto riffSize = static_cast<std::uint32_t>(file.size() - 8);
    std::memcpy(file.data() + 4, &riffSize, sizeof(riffSize));
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

static void TestParseUnknownFormats() {
    const std::uint8_t pcmGuid[16] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
    const std::uint8_t unknownGuid[16] = {0xaa, 0xbb, 0xcc, 0xdd, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc};
    Ngs2WaveformInfo info{};
    for (const std::uint32_t tag : {0x0u, 0x2u, 0x11u, 0x55u, 0x165u, 0xffffu}) {
        const auto wave = PcmFile(1, 44100, 16, 10, tag);
        Require(sceNgs2ParseWaveformData(wave.data(), wave.size(), &info) == SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);
    }
    for (const std::uint32_t formatSize : {0x10u, 0x18u, 0x26u}) {
        const auto wave = ExtensibleFile(formatSize, pcmGuid);
        Require(sceNgs2ParseWaveformData(wave.data(), wave.size(), &info) == SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);
    }
    for (const std::uint32_t formatSize : {0x28u, 0x30u, 0x36u}) {
        const auto wave = ExtensibleFile(formatSize, unknownGuid);
        Require(sceNgs2ParseWaveformData(wave.data(), wave.size(), &info) == SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);
    }
    const auto pcm = ExtensibleFile(0x34, pcmGuid);
    Require(sceNgs2ParseWaveformData(pcm.data(), pcm.size(), &info) == SCE_NGS2_OK && info.format.waveform_type == SCE_NGS2_WAVEFORM_TYPE_PCM_I16L && info.num_samples == 10);

    const auto wave = PcmFile(1, 44100, 16, 10, 0x55);
    const char* const guestPath = "/aps5_ngs2_parse_unknown.wav";
    const auto path = ResolvePath_nid_no_patch(guestPath);
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(wave.data()), static_cast<std::streamsize>(wave.size()));
    }
    Require(sceNgs2ParseWaveformFile(guestPath, 0, &info) == SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);
    std::filesystem::remove(path);
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
    const std::pair<std::uint32_t, std::uint32_t> types[] = {
        {SCE_NGS2_WAVEFORM_TYPE_PCM_U8, 1}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2},
        {SCE_NGS2_WAVEFORM_TYPE_PCM_I24L, 3}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I32L, 4},
        {SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 4}, {SCE_NGS2_WAVEFORM_TYPE_PCM_F64L, 8},
    };
    for (const auto& [type, bytes] : types) {
        for (std::uint32_t channels = 1; channels <= 8; channels++) {
            for (const std::uint32_t rate : {1u, 22050u, 48000u, 192000u}) {
                const Ngs2WaveformFormat format{type, channels, rate, 0, 0, 0};
                std::uint32_t frameSize = 0xa1, frameSamples = 0xa2, units = 0xa3, delay = 0xa4;
                Require(sceNgs2GetWaveformFrameInfo(&format, &frameSize, &frameSamples, &units, &delay) == SCE_NGS2_OK);
                Require(frameSize == channels * bytes && frameSamples == 1 && units == 1 && delay == 0);
                if (type != SCE_NGS2_WAVEFORM_TYPE_PCM_I16L) continue;
                const auto file = PcmFile(channels, rate, 16, 10);
                Ngs2WaveformInfo info{};
                Require(sceNgs2ParseWaveformData(file.data(), file.size(), &info) == SCE_NGS2_OK);
                Require(info.audio_frame_size == frameSize && info.num_audio_frame_samples == frameSamples);
                Require(info.num_audio_unit_per_frame == units && info.num_delay_samples == delay);
            }
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
    for (const std::uint32_t type : {0u, 0x10u, 0x13u, 0x19u, 0x41u, 0xffffffffu}) {
        Require(FrameInfoThrows({type, 2, 48000, 0, 0, 0}));
    }
}

static std::int32_t APS5_VABI ReadUserWaveform(std::uintptr_t user_data, std::uint32_t offset, void* data, std::size_t size) {
    const auto& file = *reinterpret_cast<const std::vector<std::uint8_t>*>(user_data);
    if (offset > file.size() || size > file.size() - offset) return -1;
    std::memcpy(data, file.data() + offset, size);
    return static_cast<std::int32_t>(size);
}

static void TestParseUser() {
    const auto file = PcmFile(2, 44100, 16, 100);
    Ngs2WaveformInfo expected{};
    Ngs2WaveformInfo actual{};
    Require(sceNgs2ParseWaveformData(file.data(), file.size(), &expected) == SCE_NGS2_OK);
    Require(sceNgs2ParseWaveformUser(reinterpret_cast<std::uintptr_t>(&ReadUserWaveform), reinterpret_cast<std::uintptr_t>(&file), &actual) == SCE_NGS2_OK);
    Require(std::memcmp(&expected, &actual, sizeof(expected)) == 0);
    Require(sceNgs2ParseWaveformUser(0, 0, &actual) == static_cast<int>(0x804A0408u));
    Require(sceNgs2ParseWaveformUser(reinterpret_cast<std::uintptr_t>(&ReadUserWaveform), reinterpret_cast<std::uintptr_t>(&file), nullptr) ==
        static_cast<int>(0x804A0053u));
    auto bad = file;
    bad[0] = 'X';
    Require(sceNgs2ParseWaveformUser(reinterpret_cast<std::uintptr_t>(&ReadUserWaveform), reinterpret_cast<std::uintptr_t>(&bad), &actual) ==
        SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);
    auto truncated = file;
    truncated.resize(16);
    Require(sceNgs2ParseWaveformUser(reinterpret_cast<std::uintptr_t>(&ReadUserWaveform), reinterpret_cast<std::uintptr_t>(&truncated), &actual) ==
        static_cast<int>(0x804A0408u));
}

int main() {
    TestParsePcm();
    TestParseUser();
    TestParseUnknownFormats();
    TestParseFile();
    TestParseFileBank();
    TestFrameInfoPcm();
    TestFrameInfoErrors();
    return 0;
}
