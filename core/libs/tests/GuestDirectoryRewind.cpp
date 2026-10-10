#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
int APS5_VABI sceKernelGetdents(int, char*, int);
std::int64_t APS5_VABI sceKernelLseek(int, std::int64_t, int);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Directory rewind check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename TFunction>
static bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static std::set<std::string> ReadAll(int directory) {
    std::set<std::string> names;
    std::array<char, 4096> buffer{};
    for (;;) {
        const int bytes = sceKernelGetdents(directory, buffer.data(), static_cast<int>(buffer.size()));
        Require(bytes >= 0);
        if (bytes == 0) return names;
        for (int offset = 0; offset < bytes;) {
            names.emplace(buffer.data() + offset + 8);
            offset += static_cast<unsigned char>(buffer[offset + 4]) | (static_cast<unsigned char>(buffer[offset + 5]) << 8);
        }
    }
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-directory-rewind-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    { std::ofstream file(root / "a"); Require(static_cast<bool>(file)); }
    { std::ofstream file(root / "b"); Require(static_cast<bool>(file)); }

    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    const std::set<std::string> expected{".", "..", "a", "b"};
    Require(ReadAll(directory) == expected);
    Require(ReadAll(directory).empty());

    Require(sceKernelLseek(directory, 0, 0) == 0);
    Require(ReadAll(directory) == expected);

    { std::ofstream file(root / "c"); Require(static_cast<bool>(file)); }
    Require(sceKernelLseek(directory, 0, 0) == 0);
    Require(ReadAll(directory) == (std::set<std::string>{".", "..", "a", "b", "c"}));

    std::array<char, 24> two{};
    Require(sceKernelLseek(directory, 0, 0) == 0);
    Require(sceKernelGetdents(directory, two.data(), static_cast<int>(two.size())) == 24);
    const auto position = sceKernelLseek(directory, 0, 1);
    Require(position == 2);
    Require(sceKernelLseek(directory, 0, 1) == 2);
    const auto rest = ReadAll(directory);
    Require(sceKernelLseek(directory, position, 0) == position);
    Require(ReadAll(directory) == rest);
    Require(!rest.contains(".") && !rest.contains(".."));

    Require(sceKernelLseek(directory, 100, 0) == 100);
    Require(sceKernelGetdents(directory, two.data(), static_cast<int>(two.size())) == 0);
    Require(sceKernelLseek(directory, 0, 1) == 100);
    Require(sceKernelLseek(directory, -98, 1) == 2);
    Require(ReadAll(directory) == rest);

    Require(sceKernelLseek(directory, 3, 0) == 3);
    Require(ThrowsRuntimeError([&] { sceKernelLseek(directory, -1, 0); }));
    Require(ThrowsRuntimeError([&] { sceKernelLseek(directory, -4, 1); }));
    Require(sceKernelLseek(directory, 0, 1) == 3);
    Require(sceKernelLseek(directory, std::numeric_limits<std::int64_t>::max(), 0) == std::numeric_limits<std::int64_t>::max());
    Require(ThrowsRuntimeError([&] { sceKernelLseek(directory, 1, 1); }));
    Require(sceKernelLseek(directory, 0, 1) == std::numeric_limits<std::int64_t>::max());

    Require(sceKernelLseek(directory, 0, 0) == 0);
    Require(sceKernelGetdents(directory, two.data(), static_cast<int>(two.size())) == 24);
    Require(sceKernelLseek(directory, 0, 1) == 2);
    const auto afterQuery = ReadAll(directory);
    Require(afterQuery == rest);

    std::array<char, 8> tiny{};
    Require(sceKernelLseek(directory, 0, 0) == 0);
    Require(sceKernelGetdents(directory, tiny.data(), static_cast<int>(tiny.size())) < 0);
    { std::ofstream file(root / "d"); Require(static_cast<bool>(file)); }
    Require(sceKernelLseek(directory, 0, 1) == 0);
    Require(ReadAll(directory) == (std::set<std::string>{".", "..", "a", "b", "c"}));
    Require(sceKernelLseek(directory, 0, 0) == 0);
    Require(sceKernelGetdents(directory, tiny.data(), static_cast<int>(tiny.size())) < 0);
    Require(sceKernelLseek(directory, 0, 0) == 0);
    Require(ReadAll(directory) == (std::set<std::string>{".", "..", "a", "b", "c", "d"}));

    Require(sceKernelClose(directory) == 0);

    const int reused = sceKernelOpen((root / "a").string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(reused == directory);
    Require(sceKernelLseek(reused, 5, 0) == 5);
    Require(sceKernelLseek(reused, 0, 1) == 5);
    Require(sceKernelLseek(reused, 0, 2) == 0);
    Require(sceKernelClose(reused) == 0);
    std::filesystem::remove_all(root);
}
