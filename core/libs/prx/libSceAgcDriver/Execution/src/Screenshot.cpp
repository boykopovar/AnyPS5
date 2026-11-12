#include "prx/libSceAgcDriver/Execution/include/Screenshot.hpp"
#include "Decoder/Png.hpp"
#include "SDL_filesystem.h"
#include "SDL_error.h"
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

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

int createExclusive(const std::filesystem::path& path) {
#ifdef _WIN32
    return ::_wopen(path.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT, _S_IREAD | _S_IWRITE);
#else
    return ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
#endif
}

bool writeAll(int descriptor, const std::uint8_t* data, std::size_t size) {
    while (size != 0) {
#ifdef _WIN32
        const unsigned int chunk = size > INT_MAX ? INT_MAX : static_cast<unsigned int>(size);
        const int written = ::_write(descriptor, data, chunk);
#else
        const auto written = ::write(descriptor, data, size);
#endif
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return false;
        data += written;
        size -= static_cast<std::size_t>(written);
    }
    return true;
}

bool closeFile(int descriptor) {
#ifdef _WIN32
    return ::_close(descriptor) == 0;
#else
    return ::close(descriptor) == 0;
#endif
}

std::filesystem::path writeUnusedPng(const std::filesystem::path& folder, const std::string& stem, const std::vector<std::uint8_t>& png) {
    for (int index = 0; index < 1000; ++index) {
        const auto path = folder / (index == 0 ? stem + ".png" : stem + "-" + std::to_string(index) + ".png");
        const int descriptor = createExclusive(path);
        if (descriptor < 0) {
            if (errno == EEXIST) continue;
            throw std::runtime_error("Screenshot: cannot create output file");
        }
        const bool written = writeAll(descriptor, png.data(), png.size());
        const bool closed = closeFile(descriptor);
        if (written && closed) return path;
        std::error_code error;
        std::filesystem::remove(path, error);
        throw std::runtime_error("Screenshot: cannot write output file");
    }
    throw std::runtime_error("Screenshot: no unused file name for " + stem + " in " + folder.string());
}

}

void AgcDriver::Screenshot::Request() {
    requested.store(true);
}

bool AgcDriver::Screenshot::Take() {
    return requested.exchange(false);
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
    return writeUnusedPng(folder, std::string(titleId) + "-" + localTimestamp(), png);
}
