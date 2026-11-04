#include "prx/libSceAgcDriver/Execution/include/Screenshot.hpp"
#include "Decoder/Png.hpp"
#include "SDL_filesystem.h"
#include "SDL_error.h"
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

std::atomic<bool> requested{false};

std::filesystem::path directory() {
#ifdef _WIN32
    const wchar_t* configured = _wgetenv(L"ANYPS5_SCREENSHOTS");
#else
    const char* configured = std::getenv("ANYPS5_SCREENSHOTS");
#endif
    if (configured != nullptr && configured[0] != 0) return configured;
    const std::unique_ptr<char, decltype(&SDL_free)> basePath(SDL_GetBasePath(), SDL_free);
    if (!basePath) throw std::runtime_error(std::string("Screenshot: cannot locate the executable: ") + SDL_GetError());
    return std::filesystem::path(reinterpret_cast<const char8_t*>(basePath.get())) / "anyps5-screenshots";
}

bool triggered() {
    static const std::filesystem::path trigger = [] {
#ifdef _WIN32
        const wchar_t* configured = _wgetenv(L"APS5_SCREENSHOT_TRIGGER");
#else
        const char* configured = std::getenv("APS5_SCREENSHOT_TRIGGER");
#endif
        return configured == nullptr ? std::filesystem::path{} : std::filesystem::path(configured);
    }();
    if (trigger.empty()) return false;
    std::error_code error;
    const auto status = std::filesystem::symlink_status(trigger, error);
    if (status.type() == std::filesystem::file_type::not_found && (!error || error == std::errc::no_such_file_or_directory)) return false;
    if (error) throw std::runtime_error("Screenshot: cannot inspect trigger file: " + error.message());
    if (!std::filesystem::is_regular_file(status)) throw std::runtime_error("Screenshot: trigger must be a regular file");
    const bool consumed = std::filesystem::remove(trigger, error);
    if (error) throw std::runtime_error("Screenshot: cannot consume trigger file: " + error.message());
    return consumed;
}

std::string localTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now) != 0) throw std::runtime_error("Screenshot: cannot read the local time");
#else
    if (localtime_r(&now, &local) == nullptr) throw std::runtime_error("Screenshot: cannot read the local time");
#endif
    char text[32];
    if (std::strftime(text, sizeof(text), "%Y%m%d-%H%M%S", &local) == 0) throw std::runtime_error("Screenshot: cannot format the local time");
    return text;
}

std::filesystem::path unusedPath(const std::filesystem::path& folder, const std::string& stem) {
    for (int index = 0; index < 1000; ++index) {
        const auto path = folder / (index == 0 ? stem + ".png" : stem + "-" + std::to_string(index) + ".png");
        if (!std::filesystem::exists(path)) return path;
    }
    throw std::runtime_error("Screenshot: no unused file name for " + stem + " in " + folder.string());
}

}

void AgcDriver::Screenshot::Request() {
    requested.store(true);
}

bool AgcDriver::Screenshot::Take() {
    const bool manual = requested.exchange(false);
    const bool file = triggered();
    return manual || file;
}

std::filesystem::path AgcDriver::Screenshot::Write(std::string_view titleId, std::uint32_t width, std::uint32_t height, std::span<const std::byte> bgra) {
    if (titleId.empty()) throw std::invalid_argument("Screenshot: empty title ID");
    const auto pixelCount = static_cast<std::size_t>(width) * height;
    if (width == 0 || height == 0 || bgra.size() != pixelCount * 4) throw std::invalid_argument("Screenshot: invalid BGRA8 frame");
    std::vector<std::uint8_t> rgb(pixelCount * 3);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        rgb[pixel * 3] = std::to_integer<std::uint8_t>(bgra[pixel * 4 + 2]);
        rgb[pixel * 3 + 1] = std::to_integer<std::uint8_t>(bgra[pixel * 4 + 1]);
        rgb[pixel * 3 + 2] = std::to_integer<std::uint8_t>(bgra[pixel * 4]);
    }
    const auto png = Decoder::Png::Encode(rgb, width, height, 3);
    const auto folder = directory();
    std::filesystem::create_directories(folder);
    const auto path = unusedPath(folder, std::string(titleId) + "-" + localTimestamp());
    std::ofstream file;
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file.open(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    file.close();
    return path;
}
