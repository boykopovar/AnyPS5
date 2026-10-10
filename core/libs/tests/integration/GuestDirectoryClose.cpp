#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelGetdents(int, char*, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI _close_nid_postfix(int);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Directory close check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr int ErrorEnotdir = static_cast<int>(0x80020014u);

static void VerifyCloseReuse(const std::filesystem::path& root, const char* scenario,
                             bool useUnderscoreClose) {
    const auto directoryPath = root / scenario;
    Require(std::filesystem::create_directory(directoryPath));
    const auto filePath = directoryPath / "entry.txt";
    {
        std::ofstream file(filePath, std::ios::binary);
        Require(file.is_open());
        file << "entry";
        Require(static_cast<bool>(file));
    }

    const int directory = sceKernelOpen(directoryPath.string().c_str(),
        SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    Require((useUnderscoreClose ? _close_nid_postfix(directory) : close_nid_postfix(directory)) == 0);

    const int file = sceKernelOpen(filePath.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(file >= 0);
    Require(file == directory);

    std::array<char, 256> entries{};
    Require(sceKernelGetdents(file, entries.data(), static_cast<int>(entries.size())) == ErrorEnotdir);
    Require((useUnderscoreClose ? _close_nid_postfix(file) : close_nid_postfix(file)) == 0);
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-directory-close-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));

    VerifyCloseReuse(root, "close", false);
    VerifyCloseReuse(root, "underscore-close", true);

    std::filesystem::remove_all(root);
}
