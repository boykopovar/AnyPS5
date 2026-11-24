#include "Ngs2Test.hpp"
#include "tests/AppArchiveFixture.hpp"

#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

static void Put16(std::vector<char>& out, std::uint32_t value) {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
}

static void Put32(std::vector<char>& out, std::uint32_t value) {
    Put16(out, value & 0xffff);
    Put16(out, value >> 16);
}

static void PutTag(std::vector<char>& out, const char* tag) {
    out.insert(out.end(), tag, tag + 4);
}

static std::vector<char> PaddedPcmFile(std::uint32_t padding, std::uint32_t sampleRate, std::uint32_t frames) {
    std::vector<char> file(padding, 0);
    PutTag(file, "RIFF");
    Put32(file, 36 + frames * 2);
    PutTag(file, "WAVE");
    PutTag(file, "fmt ");
    Put32(file, 16);
    Put16(file, 1);
    Put16(file, 1);
    Put32(file, sampleRate);
    Put32(file, sampleRate * 2);
    Put16(file, 2);
    Put16(file, 16);
    PutTag(file, "data");
    Put32(file, frames * 2);
    for (std::uint32_t i = 0; i < frames; ++i) Put16(file, i * 3);
    return file;
}

int main() {
    const auto root = AppArchiveFixture::EnterFreshRoot("anyps5-ngs2-archive");
    const auto wave = PaddedPcmFile(3, 22050, 64);
    AppArchiveFixture::WriteArchive(root / "app0.zar", {{"Sounds/Music.wav", wave}, {"Sounds/Short.wav", {'R', 'I'}}});
    { std::ofstream eboot(root / "app0" / "eboot.bin"); eboot << "elf"; }
    Require(AppArchivePath_nid_no_patch() == root / "app0.zar");

    Ngs2WaveformInfo info{};
    Require(sceNgs2ParseWaveformFile("/app0//sounds/music.wav", 3, &info) == SCE_NGS2_OK);
    Require(info.format.num_channels == 1 && info.format.sample_rate == 22050 && info.num_samples == 64);
    Require(info.data_offset == 3 + 44 && info.block[0].data_offset == 3 + 44 && info.data_size == 128);
    Require(sceNgs2ParseWaveformFile("/app0/Sounds/Music.wav", static_cast<std::uint32_t>(wave.size()) + 4, &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);
    Require(sceNgs2ParseWaveformFile("/app0/Sounds/Short.wav", 0, &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);

    bool refused = false;
    try { (void)sceNgs2ParseWaveformFile("/app0/Sounds", 0, &info); } catch (const std::runtime_error& error) { refused = std::strstr(error.what(), "cannot open") != nullptr; }
    Require(refused);

    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
