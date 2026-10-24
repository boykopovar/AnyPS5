#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
int APS5_VABI sceKernelGetdents(int, char*, int);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Directory link type check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr std::uint8_t TypeDirectory = 4;
static constexpr std::uint8_t TypeRegular = 8;
static constexpr std::uint8_t TypeLink = 10;

static std::map<std::string, std::uint8_t> Types(const char* path) {
    const int directory = sceKernelOpen(path, SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::array<char, 4096> buffer{};
    std::map<std::string, std::uint8_t> types;
    for (;;) {
        const int read = sceKernelGetdents(directory, buffer.data(), static_cast<int>(buffer.size()));
        Require(read >= 0);
        if (read == 0) break;
        for (int offset = 0; offset < read;) {
            std::uint16_t recordLength = 0;
            std::memcpy(&recordLength, buffer.data() + offset + 4, sizeof(recordLength));
            Require(recordLength != 0 && offset + recordLength <= read);
            types[std::string(buffer.data() + offset + 8)] = static_cast<std::uint8_t>(buffer[offset + 6]);
            offset += recordLength;
        }
    }
    Require(sceKernelClose(directory) == 0);
    return types;
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-directory-link-types-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(root / "app0" / "real"));
    Require(std::filesystem::create_directories(root / "dump" / "packed"));
    Require(std::filesystem::create_directories(root / "data"));
    { std::ofstream stream(root / "dump" / "eboot.bin", std::ios::binary); stream << "x"; }
    std::filesystem::create_directory_symlink(root / "dump" / "packed", root / "app0" / "packed");
    std::filesystem::create_symlink(root / "dump" / "eboot.bin", root / "app0" / "eboot.bin");
    std::filesystem::create_symlink(root / "dump" / "missing", root / "app0" / "dangling");
    std::filesystem::create_directory_symlink(root / "dump" / "packed", root / "data" / "saved");
    std::filesystem::current_path(root);

    auto app0 = Types("/app0");
    Require(app0.contains("real") && app0["real"] == TypeDirectory);
    Require(app0.contains("packed") && app0["packed"] == TypeDirectory);
    Require(app0.contains("eboot.bin") && app0["eboot.bin"] == TypeRegular);
    Require(app0.contains("dangling") && app0["dangling"] == TypeLink);

    auto data = Types("/data");
    Require(data.contains("saved") && data["saved"] == TypeLink);

    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    std::puts("directory link type tests passed");
    return 0;
}
