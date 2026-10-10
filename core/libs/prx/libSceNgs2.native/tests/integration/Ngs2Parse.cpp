#include "Ngs2Test.hpp"

#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

void Put16(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

void Put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    Put16(out, value & 0xffff);
    Put16(out, value >> 16);
}

void PutTag(std::vector<std::uint8_t>& out, const char* tag) {
    out.insert(out.end(), tag, tag + 4);
}

std::vector<std::uint8_t> PcmFile(std::uint32_t channels, std::uint32_t sampleRate, std::uint32_t bits, std::uint32_t frames) {
    const std::uint32_t frameBytes = channels * bits / 8;
    std::vector<std::uint8_t> file;
    PutTag(file, "RIFF");
    Put32(file, 36 + frames * frameBytes);
    PutTag(file, "WAVE");
    PutTag(file, "fmt ");
    Put32(file, 16);
    Put16(file, 1);
    Put16(file, channels);
    Put32(file, sampleRate);
    Put32(file, sampleRate * frameBytes);
    Put16(file, frameBytes);
    Put16(file, bits);
    PutTag(file, "data");
    Put32(file, frames * frameBytes);
    file.resize(file.size() + frames * frameBytes, 0x11);
    return file;
}

class GuestWaveFile {
public:
    GuestWaveFile(const char* guestPath, const std::vector<std::uint8_t>& wave, std::size_t padding)
        : guestPath(guestPath), path(ResolvePath_nid_no_patch(guestPath)) {
        std::ofstream out(path, std::ios::binary);
        const std::vector<char> pad(padding);
        out.write(pad.data(), static_cast<std::streamsize>(pad.size()));
        out.write(reinterpret_cast<const char*>(wave.data()), static_cast<std::streamsize>(wave.size()));
        Require(out.good(), "write the guest wave file");
    }

    ~GuestWaveFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }

    GuestWaveFile(const GuestWaveFile&) = delete;
    GuestWaveFile& operator=(const GuestWaveFile&) = delete;

    const char* const guestPath;

private:
    const std::filesystem::path path;
};

constexpr std::uint32_t filePadding = 3;

const Case parsePcm{"ParseWaveformData_StereoPcm16_ReportsFormatAndSingleBlock", [] {
    const auto file = PcmFile(2, 44100, 16, 100);
    Ngs2WaveformInfo info{};
    RequireEqual(sceNgs2ParseWaveformData(file.data(), file.size(), &info), SCE_NGS2_OK, "parse");
    RequireEqual(info.format.waveform_type, SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, "waveform type");
    RequireEqual(info.format.num_channels, 2u, "channels");
    RequireEqual(info.format.sample_rate, 44100u, "sample rate");
    RequireEqual(info.data_offset, 44u, "data offset");
    RequireEqual(info.data_size, 400u, "data size");
    RequireEqual(info.num_samples, 100u, "samples");
    RequireEqual(info.num_blocks, 1u, "blocks");
    RequireEqual(info.block[0].data_offset, 44u, "block data offset");
    RequireEqual(info.block[0].data_size, 400u, "block data size");
    RequireEqual(info.block[0].num_samples, 100u, "block samples");
}};

const Case parseTruncated{"ParseWaveformData_TruncatedData_FailsInvalidData", [] {
    auto file = PcmFile(2, 44100, 16, 100);
    file.resize(44 + 202);
    Ngs2WaveformInfo info{};
    RequireEqual(sceNgs2ParseWaveformData(file.data(), file.size(), &info), SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA, "truncated");
}};

const Case parsePartialFrame{"ParseWaveformData_DataSizeNotFrameAligned_FailsInvalidData", [] {
    auto file = PcmFile(2, 44100, 16, 100);
    file.resize(44 + 402, 0x11);
    const std::uint32_t partialSize = 402;
    std::memcpy(file.data() + 40, &partialSize, sizeof(partialSize));
    Ngs2WaveformInfo info{};
    RequireEqual(sceNgs2ParseWaveformData(file.data(), file.size(), &info), SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA, "partial frame");
}};

const Case parseUnsupportedFormat{"ParseWaveformData_EightBitOrZeroChannels_FailsInvalidFormat", [] {
    Ngs2WaveformInfo info{};
    const auto eightBit = PcmFile(1, 44100, 8, 10);
    RequireEqual(sceNgs2ParseWaveformData(eightBit.data(), eightBit.size(), &info), SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT, "8-bit");
    const auto noChannels = PcmFile(0, 44100, 16, 10);
    RequireEqual(sceNgs2ParseWaveformData(noChannels.data(), noChannels.size(), &info), SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT,
                 "zero channels");
}};

const Case parseFileArguments{"ParseWaveformFile_NullOutputOrPath_Fails", [] {
    const GuestWaveFile file("/aps5_ngs2_parse_file_arguments.wav", PcmFile(1, 22050, 16, 64), filePadding);
    Ngs2WaveformInfo info{};
    RequireEqual(sceNgs2ParseWaveformFile(file.guestPath, filePadding, nullptr), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "null output");
    RequireEqual(sceNgs2ParseWaveformFile(nullptr, 0, &info), SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA, "null path");
}};

const Case parseFileAtOffset{"ParseWaveformFile_OffsetHeader_ReportsOffsetsIncludingPadding", [] {
    const GuestWaveFile file("/aps5_ngs2_parse_file_offset.wav", PcmFile(1, 22050, 16, 64), filePadding);
    Ngs2WaveformInfo info{};
    RequireEqual(sceNgs2ParseWaveformFile(file.guestPath, filePadding, &info), SCE_NGS2_OK, "parse");
    RequireEqual(info.format.num_channels, 1u, "channels");
    RequireEqual(info.format.sample_rate, 22050u, "sample rate");
    RequireEqual(info.num_samples, 64u, "samples");
    RequireEqual(info.data_offset, filePadding + 44, "data offset");
    RequireEqual(info.block[0].data_offset, filePadding + 44, "block data offset");
    RequireEqual(info.data_size, 128u, "data size");
}};

const Case parseFileBadOffset{"ParseWaveformFile_OffsetPastEndOrBeforeHeader_Fails", [] {
    const auto wave = PcmFile(1, 22050, 16, 64);
    const GuestWaveFile file("/aps5_ngs2_parse_file_bad_offset.wav", wave, filePadding);
    Ngs2WaveformInfo info{};
    RequireEqual(sceNgs2ParseWaveformFile(file.guestPath, static_cast<std::uint32_t>(wave.size()) + 4, &info),
                 SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA, "offset past the end");
    RequireEqual(sceNgs2ParseWaveformFile(file.guestPath, 0, &info), SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT, "offset before header");
}};

} // namespace
