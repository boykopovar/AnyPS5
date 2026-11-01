#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Positioned race check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)
int main() {
    const auto root = std::filesystem::path("anyps5-positioned-race-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto file = root / "blocks.bin";
    constexpr std::int64_t kBlocks = 8;
    constexpr std::size_t kBlockSize = 4096;
    {
        std::ofstream stream(file, std::ios::binary);
        Require(static_cast<bool>(stream));
        for (std::int64_t block = 0; block < kBlocks; ++block) {
            stream << std::string(kBlockSize, static_cast<char>('A' + block));
        }
    }
    const int fd = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(fd >= 0);
    std::array<char, 64> expected{};
    expected.fill('A');
    std::atomic<bool> stop{false};
    std::atomic<bool> misread{false};
    std::array<char, 64> seeked{};
    seeked.fill('H');
    std::thread seeker([&] {
        std::array<char, 64> sink{};
        while (!stop.load()) {
            if (sceKernelLseek(fd, (kBlocks - 1) * kBlockSize, 0) != (kBlocks - 1) * kBlockSize) {
                misread = true;
                return;
            }
            if (sceKernelRead(fd, sink.data(), sink.size()) != static_cast<std::int64_t>(sink.size()) ||
                std::memcmp(sink.data(), seeked.data(), seeked.size()) != 0) {
                misread = true;
                return;
            }
        }
    });
    for (int i = 0; i < 5000 && !misread.load(); ++i) {
        std::array<char, 64> bytes{};
        const auto got = sceKernelPread(fd, bytes.data(), bytes.size(), 0);
        if (got != static_cast<std::int64_t>(bytes.size()) || std::memcmp(bytes.data(), expected.data(), expected.size()) != 0) {
            misread = true;
        }
    }
    stop = true;
    seeker.join();
    Require(!misread);
    Require(sceKernelClose(fd) == 0);
    Require(std::filesystem::remove_all(root) > 0);
}
