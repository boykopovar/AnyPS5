#include <io/FileIdentity.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream(path, std::ios::binary) << text;
    require(std::filesystem::is_regular_file(path), "Cannot create " + path.string());
}

void requireMissing(const std::filesystem::path& first, const std::filesystem::path& second) {
    try {
        Io::SameFile(first, second);
    } catch (const std::filesystem::filesystem_error&) {
        return;
    }
    throw std::runtime_error("SameFile accepted a missing file");
}

void run(const std::filesystem::path& directory) {
    const auto first = directory / "first.bin";
    const auto link = directory / "link.bin";
    const auto copy = directory / "copy.bin";
    const auto folder = directory / "folder";
    write(first, "same");
    write(copy, "same");
    std::filesystem::create_hard_link(first, link);
    std::filesystem::create_directory(folder);
    require(Io::SameFile(first, first), "A file differs from itself");
    require(Io::SameFile(first, folder / ".." / "first.bin"), "Two spellings of one file differ");
    require(Io::SameFile(first, link), "A hard link differs from its target");
    require(!Io::SameFile(first, copy), "A copy with the same contents is the same file");
    require(Io::SameFile(folder, directory / "folder" / "."), "Two spellings of one directory differ");
    require(!Io::SameFile(first, folder), "A file is the same as a directory");
    requireMissing(directory / "missing.bin", first);
    requireMissing(first, directory / "missing.bin");
}

}

int main() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("anyps5-file-identity-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directories(directory);
        run(directory);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
        return 1;
    }
    std::filesystem::remove_all(directory);
    return 0;
}
