#include "prx/libSceVideoRecording/libSceVideoRecording.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "tests/ProcessParametersFixture.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr int kInvalidArgument = static_cast<int>(0x80a80003u);
constexpr int kNotOpen = static_cast<int>(0x80a80006u);

alignas(16) std::array<std::byte, 0x40> process{};

void Require(bool value) { if (!value) std::abort(); }

void Put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}

void WriteSfo(const std::filesystem::path& root) {
    const auto metadata = root / "app0" / "sce_sys";
    std::filesystem::create_directories(metadata);
    const std::array<std::string, 2> keys{"TITLE", "TITLE_ID"};
    const std::array<std::string, 2> text{"Video recording test", "CUSA00000"};
    std::vector<std::uint8_t> entries(20 + keys.size() * 16);
    std::vector<std::uint8_t> names;
    std::vector<std::uint8_t> data;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto offset = 20 + i * 16;
        const auto nameOffset = static_cast<std::uint16_t>(names.size());
        std::memcpy(entries.data() + offset, &nameOffset, sizeof(nameOffset));
        const std::uint16_t format = 0x0204;
        std::memcpy(entries.data() + offset + 2, &format, sizeof(format));
        const auto length = static_cast<std::uint32_t>(text[i].size() + 1);
        Put32(entries, offset + 4, length);
        Put32(entries, offset + 8, length);
        Put32(entries, offset + 12, static_cast<std::uint32_t>(data.size()));
        names.insert(names.end(), keys[i].begin(), keys[i].end());
        names.push_back(0);
        data.insert(data.end(), text[i].begin(), text[i].end());
        data.push_back(0);
    }
    while (names.size() % 4 != 0) names.push_back(0);
    std::memcpy(entries.data(), "\0PSF", 4);
    Put32(entries, 4, 0x101);
    Put32(entries, 8, static_cast<std::uint32_t>(entries.size()));
    Put32(entries, 12, static_cast<std::uint32_t>(entries.size() + names.size()));
    Put32(entries, 16, static_cast<std::uint32_t>(keys.size()));
    entries.insert(entries.end(), names.begin(), names.end());
    entries.insert(entries.end(), data.begin(), data.end());
    std::ofstream file(metadata / "param.sfo", std::ios::binary);
    file.write(reinterpret_cast<const char*>(entries.data()), static_cast<std::streamsize>(entries.size()));
    Require(static_cast<bool>(file));
}

}

int main() {
    const std::uint64_t size = process.size();
    std::memcpy(process.data(), &size, sizeof(size));
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    const std::uint32_t sdkVersion = 0x09008001;
    std::memcpy(process.data() + 0x10, &sdkVersion, sizeof(sdkVersion));
    const auto original = std::filesystem::current_path();
    const auto directory = std::filesystem::temp_directory_path() / ("video_recording_ps4_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    WriteSfo(directory);
    std::filesystem::current_path(directory);
    Require(sceKernelIsPs4Process() == 1);

    std::array<unsigned char, 0x100> data{};
    data.fill(0xab);
    for (const std::int32_t info : {2, 6, 7, 8, 0xd, 0x8001, 0x8002, 0x8003, 0x8004, 0x8005, 0xa01, 0xa007, 0xa008, 0xa009, 0xa00c}) {
        Require(sceVideoRecordingSetInfo_nid_disambig1(info, data.data(), sizeof(std::uint32_t)) == kInvalidArgument);
        Require(sceVideoRecordingSetInfo_nid_disambig1(info, data.data(), data.size()) == kInvalidArgument);
        Require(sceVideoRecordingSetInfo_nid_disambig1(info, nullptr, 0) == kInvalidArgument);
    }
    for (const std::int32_t info : {0x9004, 0xa004, 0xa006}) {
        Require(sceVideoRecordingSetInfo_nid_disambig1(info, data.data(), sizeof(std::uint32_t)) == kNotOpen);
        Require(sceVideoRecordingSetInfo_nid_disambig1(info, nullptr, 0) == kNotOpen);
    }
    for (const std::int32_t info : {-1, 0, 1, 3, 4, 5, 9, 0xa, 0xb, 0xc, 0xe, 0x8000, 0x8006, 0x9003, 0x9005, 0xa005, 0xa00a, 0xa00b, 0xa00d}) {
        Require(sceVideoRecordingSetInfo_nid_disambig1(info, data.data(), data.size()) == kInvalidArgument);
    }
    for (const auto byte : data) Require(byte == 0xab);

    std::filesystem::current_path(original);
    std::filesystem::remove_all(directory);
    return 0;
}
