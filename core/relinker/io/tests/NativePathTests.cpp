#include <io/FileReader.hpp>
#include <io/FileWriter.hpp>
#include <io/NativePath.hpp>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void RequireInvalidPath(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument& error) {
        Require(std::string(error.what()).find("NUL") != std::string::npos, "Path error does not identify the NUL byte");
        return;
    }
    throw std::runtime_error("A path containing a NUL byte was accepted");
}

void Conversions() {
    for (const std::string path : {"", "relative/file.bin", "a path with spaces", "caf\xc3\xa9/\xe6\x97\xa5\xe6\x9c\xac.bin"})
        Require(Io::Utf8Path(Io::NativePath(path)) == path, "Valid path did not round-trip");
    for (const std::string prefix : {"", "file", "directory/file"}) {
        for (const auto& suffix : std::vector<std::string>{"", "suffix", std::string(1, '\0')}) {
            const auto path = prefix + '\0' + suffix;
            RequireInvalidPath([&] { Io::NativePath(path); });
        }
    }
}

void FileAccess(const std::filesystem::path& directory) {
    const auto original = directory / Io::NativePath("caf\xc3\xa9-\xe6\x97\xa5\xe6\x9c\xac.bin");
    const auto originalPath = Io::Utf8Path(original);
    const std::vector<std::uint8_t> expected{0, 1, 10, 13, 127, 128, 255};
    Io::FileWriter().Write(originalPath, expected);
    Require(Io::FileReader().Read(originalPath) == expected, "Valid UTF-8 file path did not preserve binary data");
    const auto missingPath = Io::Utf8Path(directory / "missing.bin");
    for (const auto& prefix : {originalPath, missingPath}) {
        for (const std::string suffix : {"", ".other", "/other.bin"}) {
            const auto path = prefix + '\0' + suffix;
            RequireInvalidPath([&] { Io::FileReader().Read(path); });
            RequireInvalidPath([&] { Io::FileWriter().Write(path, std::vector<std::uint8_t>{42}); });
            RequireInvalidPath([&] { Io::FileWriter().Write(path, std::string("replacement")); });
            Require(Io::FileReader().Read(originalPath) == expected, "Invalid path changed the file named before the NUL byte");
            Require(!std::filesystem::exists(Io::NativePath(missingPath)), "Invalid path created the file named before the NUL byte");
        }
    }
}

}

int main() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("anyps5-native-path-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        Conversions();
        std::filesystem::create_directory(directory);
        FileAccess(directory);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
        return 1;
    }
    std::filesystem::remove_all(directory);
}
