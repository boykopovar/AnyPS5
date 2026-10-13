#include "prx/libSceAgcDriver/Execution/include/Screenshot.hpp"
#include "Decoder/Png.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void setScreenshotDirectory(const std::filesystem::path& directory) {
#ifdef _WIN32
    require(_putenv_s("ANYPS5_SCREENSHOTS", directory.string().c_str()) == 0, "set ANYPS5_SCREENSHOTS");
#else
    require(::setenv("ANYPS5_SCREENSHOTS", directory.string().c_str(), 1) == 0, "set ANYPS5_SCREENSHOTS");
#endif
}

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "open the written screenshot");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void requestsMerge() {
    require(!AgcDriver::Screenshot::Take(), "no screenshot is requested at start");
    AgcDriver::Screenshot::Request();
    AgcDriver::Screenshot::Request();
    require(AgcDriver::Screenshot::Take(), "a requested screenshot is taken");
    require(!AgcDriver::Screenshot::Take(), "repeated requests are taken once");
}

void writesRgbPng(const std::filesystem::path& directory) {
    const std::array<std::byte, 8> bgra{std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x00}, std::byte{0xA0}, std::byte{0xB0}, std::byte{0xC0}, std::byte{0x7F}};
    const auto first = AgcDriver::Screenshot::Write("TEST00001", 2, 1, bgra);
    const auto second = AgcDriver::Screenshot::Write("TEST00001", 2, 1, bgra);
    require(first.parent_path() == directory, "the screenshot is written to ANYPS5_SCREENSHOTS");
    require(first.extension() == ".png" && first.filename().string().starts_with("TEST00001-"), "the screenshot is named after the title ID");
    require(first != second && std::filesystem::exists(second), "a second screenshot does not replace the first");
    const auto image = Decoder::Png::Decode(readFile(first));
    require(image.has_value() && image->width == 2 && image->height == 1, "the screenshot decodes at the frame extent");
    const std::vector<std::uint8_t> expected{0x30, 0x20, 0x10, 0xFF, 0xC0, 0xB0, 0xA0, 0xFF};
    require(image->pixels == expected, "the screenshot holds the frame's colors as opaque RGB");
}

void rejectsInvalidFrames() {
    const std::array<std::byte, 4> pixel{};
    const auto rejects = [](auto write) {
        try {
            write();
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    require(rejects([&] { AgcDriver::Screenshot::Write("TEST00001", 2, 1, pixel); }), "a frame smaller than its extent is rejected");
    require(rejects([&] { AgcDriver::Screenshot::Write("TEST00001", 0, 1, {}); }), "an empty frame is rejected");
    require(rejects([&] { AgcDriver::Screenshot::Write("", 1, 1, pixel); }), "an empty title ID is rejected");
}

}

int main() try {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5_screenshot-" + std::to_string(std::random_device{}()));
    const auto directory = root / "nested";
    std::filesystem::remove_all(root);
    setScreenshotDirectory(directory);
    requestsMerge();
    writesRgbPng(directory);
    rejectsInvalidFrames();
    std::filesystem::remove_all(root);
    std::cout << "agc screenshot tests passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
