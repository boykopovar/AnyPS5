#include "prx/libSceScreenShot/include/Api.hpp"
#include "ProcessParametersFixture.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace {

constexpr int ErrorArgument = static_cast<int>(0x80BE0001);
constexpr int ErrorOverlayFile = static_cast<int>(0x80BE0003);
constexpr int ErrorOverlaySize = static_cast<int>(0x80BE000E);

alignas(16) std::array<std::byte, 0x40> process{};
std::string pathPrefix;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ScreenShot: %s\n", message);
    std::abort();
}

void InstallProcessParameters(std::uint32_t sdkVersion) {
    const std::uint64_t size = process.size();
    const std::uint32_t magic = 0x4942524f;
    std::memcpy(process.data(), &size, sizeof(size));
    std::memcpy(process.data() + 8, &magic, sizeof(magic));
    std::memcpy(process.data() + 0x10, &sdkVersion, sizeof(sdkVersion));
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "process parameters were not installed");
}

class TempPath {
public:
    explicit TempPath(const char* tag) : name(pathPrefix + tag) {}
    ~TempPath() {
        std::error_code error;
        std::filesystem::remove_all(name, error);
    }
    const char* c_str() const { return name.c_str(); }
    void Fill(std::size_t bytes) const {
        std::ofstream file(name, std::ios::binary | std::ios::trunc);
        const std::string chunk(4096, 'x');
        for (std::size_t written = 0; written < bytes; written += chunk.size()) {
            file.write(chunk.data(), static_cast<std::streamsize>(std::min(chunk.size(), bytes - written)));
        }
    }

private:
    std::string name;
};

void TestDisableState() {
    int value = -1;
    Require(sceScreenShotIsDisabled(0, &value) == 0 && value == 0, "capture starts enabled");
    Require(sceScreenShotDisable() == 0, "disable");
    Require(sceScreenShotIsDisabled(0, &value) == 0 && value == 1, "disable is reported");
    Require(sceScreenShotEnable() == 0, "enable");
    Require(sceScreenShotIsDisabled(0, &value) == 0 && value == 0, "enable is reported");
    value = 7;
    Require(sceScreenShotIsDisabled(-1, &value) == ErrorArgument && value == 7, "id -1 is rejected");
    Require(sceScreenShotIsDisabled(0, nullptr) == ErrorArgument, "null result is rejected");
    Require(sceScreenShotDisableNotification() == 0 && sceScreenShotEnableNotification() == 0, "notification toggles");
}

void TestOverlay(bool large) {
    const int maxX = large ? 3839 : 1919;
    const int maxY = large ? 2159 : 1079;
    const std::size_t maxSize = large ? 8u << 20 : 2u << 20;
    TempPath image("image");
    image.Fill(4096);
    Require(sceScreenShotSetOverlayImage(image.c_str(), 0, 0) == 0, "overlay at the origin");
    Require(sceScreenShotSetOverlayImage(image.c_str(), maxX, maxY) == 0, "overlay at the last offsets");
    Require(sceScreenShotSetOverlayImage(image.c_str(), maxX + 1, 0) == ErrorArgument, "x past the screen");
    Require(sceScreenShotSetOverlayImage(image.c_str(), 0, maxY + 1) == ErrorArgument, "y past the screen");
    Require(sceScreenShotSetOverlayImage(image.c_str(), -1, 0) == ErrorArgument, "negative x");
    Require(sceScreenShotSetOverlayImage(nullptr, 0, 0) == ErrorArgument, "null path");
    const std::string longPath(1024, 'a');
    Require(sceScreenShotSetOverlayImage(longPath.c_str(), 0, 0) == ErrorArgument, "path of 1024 characters");
    TempPath missing("missing");
    Require(sceScreenShotSetOverlayImage(missing.c_str(), 0, 0) == ErrorOverlayFile, "missing file");
    TempPath directory("directory");
    std::filesystem::create_directory(directory.c_str());
    Require(sceScreenShotSetOverlayImage(directory.c_str(), 0, 0) == ErrorOverlayFile, "directory");
    TempPath empty("empty");
    empty.Fill(0);
    Require(sceScreenShotSetOverlayImage(empty.c_str(), 0, 0) == ErrorOverlaySize, "empty file");
    TempPath full("full");
    full.Fill(maxSize);
    Require(sceScreenShotSetOverlayImage(full.c_str(), 0, 0) == 0, "file at the size limit");
    full.Fill(maxSize + 1);
    Require(sceScreenShotSetOverlayImage(full.c_str(), 0, 0) == ErrorOverlaySize, "file past the size limit");
    for (int origin = 1; origin <= 9; ++origin) Require(sceScreenShotSetOverlayImageWithOrigin(image.c_str(), 0, 0, origin) == 0, "origins 1 to 9");
    Require(sceScreenShotSetOverlayImageWithOrigin(image.c_str(), 0, 0, 0) == ErrorArgument, "origin 0");
    Require(sceScreenShotSetOverlayImageWithOrigin(image.c_str(), 0, 0, 10) == ErrorArgument, "origin 10");
    Require(sceScreenShotSetOverlayImageWithOrigin(nullptr, 0, 0, 0) == ErrorArgument, "origin checked first");
}

}

int main(int argc, char** argv) {
    const bool large = argc > 1 && std::strcmp(argv[1], "large") == 0;
    pathPrefix = std::string("anyps5-screenshot-") + (large ? "large-" : "small-") + std::to_string(std::random_device{}()) + "-";
    InstallProcessParameters(large ? 0x3500000u : 0x34fffffu);
    TestDisableState();
    TestOverlay(large);
    return 0;
}
