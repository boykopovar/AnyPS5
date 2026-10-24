#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <random>
#include <source_location>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataMount3(const SaveDataMount3*, SaveDataMountResult*);
int APS5_VABI sceSaveDataUmount2(std::uint32_t, const SaveDataMountPoint*);
int APS5_VABI sceSaveDataLoadIcon(const SaveDataMountPoint*, SaveDataIcon*);
int APS5_VABI sceSaveDataSaveIcon(const SaveDataMountPoint*, const SaveDataIcon*);
}

static constexpr int Parameter = static_cast<int>(0x809F0000);
static constexpr int NotMounted = static_cast<int>(0x809F0004);
static constexpr int NotFound = static_cast<int>(0x809F0008);
static constexpr int Internal = static_cast<int>(0x809F000B);
static constexpr int BadMounted = static_cast<int>(0x809F000D);
static constexpr std::uint32_t ReadOnly = 1;
static constexpr std::uint32_t ReadWrite = 2;
static constexpr std::uint32_t Create = 32;

static void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "save icon requirement failed at %s:%u\n", location.file_name(), location.line());
    throw std::runtime_error("savedata icon requirement failed");
}

static SaveDataMountPoint Mount(const char* name, std::uint32_t mode = ReadWrite | Create) {
    SceSaveDataDirName directory{};
    Require(std::strlen(name) < sizeof(directory.data));
    std::memcpy(directory.data, name, std::strlen(name) + 1);
    SaveDataMount3 request{};
    request.dir_name = &directory;
    request.mount_mode = mode;
    SaveDataMountResult result{};
    Require(sceSaveDataMount3(&request, &result) == 0);
    return result.mount_point;
}

static void Unmount(const SaveDataMountPoint& point) {
    Require(sceSaveDataUmount2(0, &point) == 0);
}

static std::array<unsigned char, 97> Payload() {
    std::array<unsigned char, 97> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<unsigned char>(i * 43);
    return bytes;
}

static void Save(const SaveDataMountPoint& point, void* data, std::size_t capacity, std::size_t size) {
    SaveDataIcon icon{};
    icon.buf = data;
    icon.buf_size = capacity;
    icon.data_size = size;
    std::fill(std::begin(icon.reserved), std::end(icon.reserved), 0x5A);
    const auto before = icon;
    Require(sceSaveDataSaveIcon(&point, &icon) == 0);
    Require(std::memcmp(&icon, &before, sizeof(icon)) == 0);
}

static void Load(const SaveDataMountPoint& point, const unsigned char* expected, std::size_t size,
                 std::size_t capacity = 101) {
    std::array<unsigned char, 113> bytes{};
    bytes.fill(0xCD);
    Require(capacity <= bytes.size() - 8);
    SaveDataIcon icon{};
    icon.buf = bytes.data() + 4;
    icon.buf_size = capacity;
    icon.data_size = 0x123456;
    std::fill(std::begin(icon.reserved), std::end(icon.reserved), 0x5A);
    Require(sceSaveDataLoadIcon(&point, &icon) == 0);
    Require(icon.buf == bytes.data() + 4 && icon.buf_size == capacity && icon.data_size == size);
    Require(std::all_of(std::begin(icon.reserved), std::end(icon.reserved), [](auto byte) { return byte == 0x5A; }));
    const auto copied = std::min(capacity, size);
    Require(std::equal(expected, expected + copied, bytes.begin() + 4));
    Require(std::all_of(bytes.begin(), bytes.begin() + 4, [](auto byte) { return byte == 0xCD; }));
    Require(std::all_of(bytes.begin() + 4 + copied, bytes.end(), [](auto byte) { return byte == 0xCD; }));
}

static void CheckRoundTrip() {
    auto payload = Payload();
    const auto first = Mount("first");
    Save(first, payload.data(), payload.size(), payload.size() - 3);
    const auto iconPath = std::filesystem::path("_sd") / "first" / "sce_sys" / "icon0.png";
    Require(std::filesystem::is_regular_file(iconPath));
    Require(std::filesystem::file_size(iconPath) == payload.size() - 3);
    Load(first, payload.data(), payload.size() - 3);
    const auto directory = std::filesystem::current_path();
    Require(std::filesystem::create_directory("changed-cwd"));
    std::filesystem::current_path(directory / "changed-cwd");
    Load(first, payload.data(), payload.size() - 3);
    std::filesystem::current_path(directory);
    const auto second = Mount("second");
    auto other = payload;
    std::reverse(other.begin(), other.end());
    Save(second, other.data(), other.size(), other.size());
    Load(first, payload.data(), payload.size() - 3);
    Load(second, other.data(), other.size());
    Unmount(first);
    const auto remounted = Mount("first", ReadWrite);
    Load(remounted, payload.data(), payload.size() - 3);
    Unmount(remounted);
    Unmount(second);
}

static void CheckBounds() {
    auto payload = Payload();
    const auto point = Mount("bounds");
    Save(point, payload.data(), payload.size(), payload.size());
    Load(point, payload.data(), payload.size(), 13);
    Load(point, payload.data(), payload.size(), 0);
    Save(point, payload.data(), 7, payload.size());
    Load(point, payload.data(), 7);
    Save(point, payload.data(), payload.size(), 0);
    Load(point, payload.data(), 0);
    Save(point, payload.data(), 0, payload.size());
    Load(point, payload.data(), 0);
    Unmount(point);
}

static void CheckReadOnly() {
    auto payload = Payload();
    auto point = Mount("readonly");
    Save(point, payload.data(), payload.size(), payload.size());
    Unmount(point);
    point = Mount("readonly", ReadOnly);
    SaveDataIcon icon{};
    icon.buf = payload.data();
    icon.buf_size = payload.size();
    icon.data_size = 1;
    Require(sceSaveDataSaveIcon(&point, &icon) == BadMounted);
    Load(point, payload.data(), payload.size());
    Unmount(point);
    point = Mount("readonly", ReadWrite);
    Save(point, payload.data(), payload.size(), 7);
    Load(point, payload.data(), 7);
    Unmount(point);
}

static void CheckArguments() {
    auto payload = Payload();
    const auto point = Mount("arguments");
    SaveDataIcon icon{};
    icon.buf = payload.data();
    icon.buf_size = payload.size();
    icon.data_size = 0x123456;
    Require(sceSaveDataLoadIcon(nullptr, &icon) == Parameter);
    Require(sceSaveDataSaveIcon(nullptr, &icon) == Parameter);
    Require(sceSaveDataLoadIcon(&point, nullptr) == Parameter);
    Require(sceSaveDataSaveIcon(&point, nullptr) == Parameter);
    SaveDataIcon empty{};
    empty.data_size = 0x123456;
    Require(sceSaveDataLoadIcon(&point, &empty) == Parameter);
    Require(sceSaveDataSaveIcon(&point, &empty) == Parameter);
    Require(empty.data_size == 0x123456);
    Require(sceSaveDataLoadIcon(&point, &icon) == NotFound);
    Require(icon.data_size == 0x123456);
    SaveDataMountPoint absent{};
    std::memcpy(absent.data, "absent", sizeof("absent"));
    Require(sceSaveDataLoadIcon(&absent, &icon) == NotMounted);
    Require(sceSaveDataSaveIcon(&absent, &icon) == NotMounted);
    Unmount(point);
    Require(sceSaveDataLoadIcon(&point, &icon) == NotMounted);
    Require(sceSaveDataSaveIcon(&point, &icon) == NotMounted);
}

static void CheckIoFailure() {
    auto payload = Payload();
    const auto point = Mount("failure");
    const auto iconPath = std::filesystem::path("_sd") / "failure" / "sce_sys" / "icon0.png";
    Require(std::filesystem::create_directories(iconPath));
    SaveDataIcon icon{};
    icon.buf = payload.data();
    icon.buf_size = payload.size();
    icon.data_size = 19;
    Require(sceSaveDataSaveIcon(&point, &icon) == Internal);
    Require(sceSaveDataLoadIcon(&point, &icon) == Internal);
    Require(icon.data_size == 19);
    Require(std::filesystem::is_directory(iconPath));
    Require(std::distance(std::filesystem::directory_iterator(iconPath.parent_path()), std::filesystem::directory_iterator{}) == 1);
    Unmount(point);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const auto previous = std::filesystem::current_path();
    const auto parent = std::filesystem::temp_directory_path();
    std::filesystem::path root;
    for (unsigned attempt = 0; attempt != 16; ++attempt) {
        const auto candidate = parent / ("anyps5-savedata-icon-" + std::to_string(std::random_device{}()));
        if (std::filesystem::create_directory(candidate)) { root = candidate; break; }
    }
    Require(!root.empty());
    const std::string operation(argv[1]);
    int status = 0;
    try {
        std::filesystem::current_path(root);
        Require(sceSaveDataInitialize3(nullptr) == 0);
        if (operation == "roundtrip") CheckRoundTrip();
        else if (operation == "bounds") CheckBounds();
        else if (operation == "readonly") CheckReadOnly();
        else if (operation == "arguments") CheckArguments();
        else { Require(operation == "io"); CheckIoFailure(); }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        status = 1;
    }
    std::filesystem::current_path(previous);
    Require(std::filesystem::weakly_canonical(root).parent_path() == std::filesystem::weakly_canonical(parent));
    std::error_code error;
    std::filesystem::remove_all(root, error);
    Require(!error);
    if (status == 0) std::printf("savedata icon %s passed\n", argv[1]);
    return status;
}
