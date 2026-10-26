#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Positioned close check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr std::int64_t ErrorEbadf = static_cast<std::int32_t>(0x80020009u);

int main() {
    const auto root = std::filesystem::path("anyps5-positioned-close-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto path = (root / "data.bin").string();
    constexpr std::size_t fileSize = 48u << 20;
    {
        std::ofstream stream(path, std::ios::binary);
        const std::vector<char> block(1u << 20, 'x');
        for (std::size_t written = 0; written < fileSize; written += block.size()) stream.write(block.data(), static_cast<std::streamsize>(block.size()));
        Require(stream.good());
    }
    std::vector<char> buffer(32u << 20);
    int overlapped = 0;
    for (int round = 0; round < 60; ++round) {
        const int descriptor = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
        Require(descriptor >= 0);
        std::atomic<int> state{0};
        std::atomic<bool> threw{false};
        std::int64_t result = 0;
        std::thread reader([&] {
            state = 1;
            try {
                result = sceKernelPread(descriptor, buffer.data(), buffer.size(), 0);
            } catch (const std::exception&) {
                threw = true;
            }
            state = 2;
        });
        while (state.load() == 0) std::this_thread::yield();
        PreciseSleepUs(300);
        const int closed = sceKernelClose(descriptor);
        reader.join();
        Require(!threw.load());
        Require(closed == 0);
        Require(result == static_cast<std::int64_t>(buffer.size()) || result == ErrorEbadf);
        if (result == static_cast<std::int64_t>(buffer.size())) ++overlapped;
    }
    Require(overlapped > 0);
    std::filesystem::remove_all(root);
    return 0;
}
