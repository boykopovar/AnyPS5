#include <NativeModules.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::uint8_t> fixture(const std::string& needed = {}, const std::string& soname = {}) {
    std::vector<std::uint8_t> bytes(0x3000);
    const std::array<std::uint8_t, 7> magic{0x7f, 'E', 'L', 'F', 2, 1, 1};
    std::copy(magic.begin(), magic.end(), bytes.begin());
    Io::WriteU16(bytes, 16, 3);
    Io::WriteU16(bytes, 18, 62);
    Io::WriteU32(bytes, 20, 1);
    Io::WriteU64(bytes, 24, 0x1000);
    Io::WriteU64(bytes, 32, 64);
    Io::WriteU16(bytes, 52, 64);
    Io::WriteU16(bytes, 54, 56);
    Io::WriteU16(bytes, 56, 8);
    Io::WriteU16(bytes, 58, 64);
    const auto header = [&](std::size_t index, unsigned type, unsigned flags, unsigned offset, unsigned size) {
        const auto at = 64 + index * 56;
        Io::WriteU32(bytes, at, type);
        Io::WriteU32(bytes, at + 4, flags);
        Io::WriteU64(bytes, at + 8, offset);
        Io::WriteU64(bytes, at + 16, offset);
        Io::WriteU64(bytes, at + 24, offset);
        Io::WriteU64(bytes, at + 32, size);
        Io::WriteU64(bytes, at + 40, size);
        Io::WriteU64(bytes, at + 48, 8);
    };
    header(0, 1, 5, 0x1000, 8);
    header(1, 1, 6, 0x2000, 0x1000);
    for (std::size_t index = 3; index < 8; ++index) header(index, 0x6fffff01, 0, 0, 0);
    bytes[0x1000] = 0xc3;
    const std::size_t sonameOffset = needed.empty() ? 1 : needed.size() + 2;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> tags{{5, 0x2600}, {10, soname.empty() ? sonameOffset : sonameOffset + soname.size() + 1}, {6, 0x2620}, {11, 24}, {7, 0x2700}, {8, 0}, {9, 24}, {4, 0x2680}};
    if (!needed.empty()) {
        std::copy(needed.begin(), needed.end(), bytes.begin() + 0x2601);
        tags.push_back({1, 1});
    }
    if (!soname.empty()) {
        std::copy(soname.begin(), soname.end(), bytes.begin() + 0x2600 + static_cast<std::ptrdiff_t>(sonameOffset));
        tags.push_back({14, sonameOffset});
    }
    tags.push_back({0, 0});
    header(2, 2, 6, 0x2000, tags.size() * 16);
    for (std::size_t index = 0; index < tags.size(); ++index) {
        Io::WriteU64(bytes, 0x2000 + index * 16, tags[index].first);
        Io::WriteU64(bytes, 0x2008 + index * 16, tags[index].second);
    }
    Io::WriteU32(bytes, 0x2680, 1);
    Io::WriteU32(bytes, 0x2684, 1);
    return bytes;
}

void write(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(stream.good(), "a fixture file could not be written");
}

}

int main() {
    try {
        const auto directory = std::filesystem::temp_directory_path() / "anyps5-native-modules-test";
        std::filesystem::remove_all(directory);
        const auto root = directory / "root";
        const auto modules = root / "sce_module";
        std::filesystem::create_directories(modules);
        const auto input = directory / "eboot.elf";
        write(input, {0x7f, 'E', 'L', 'F'});

        std::vector<std::uint8_t> self(18, 0);
        self[0] = 0x4f;
        self[1] = 0x15;
        self[2] = 0x3d;
        self[3] = 0x1d;
        write(modules / "plugin.prx", self);

        const auto elf = fixture({}, "plugin.prx");
        write(modules / "plugin.prx.esbak", elf);

        const auto read = Cli::ReadModules(input, root, {}, {});
        require(read.Modules.size() == 1, "the SELF and its .esbak were not one module");
        require(read.Modules.front().Path.filename() == "plugin.prx", "the module kept the .esbak output name");
        require(read.Modules.front().Bytes == elf, "the module did not use the .esbak bytes");

        std::filesystem::remove_all(directory);
        std::cout << "native_modules: ok\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native_modules: " << error.what() << "\n";
        return 1;
    }
}
