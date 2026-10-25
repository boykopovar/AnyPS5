#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iterator>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

#undef main

namespace {

constexpr std::uint32_t Width = 3840;
constexpr std::uint32_t Height = 2160;
std::uint32_t scaleFromEnvironment() {
    const char* value = std::getenv("APS5_DUMP_SCALE");
    return value != nullptr ? static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10)) : 4u;
}
const std::uint32_t Scale = scaleFromEnvironment();
const std::uint32_t DumpWidth = Width / Scale;
const std::uint32_t DumpHeight = Height / Scale;
const std::size_t DumpBytes = 54 + static_cast<std::size_t>(DumpWidth) * DumpHeight * 4;
constexpr int Frames = 4;
bool WaitMode = false;

void check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::string frameName(int index) {
    char name[32];
    std::snprintf(name, sizeof(name), "frame_%03d.bmp", index);
    return name;
}

class HeldDump final {
    int descriptor;

public:
    explicit HeldDump(int index) {
        const auto name = frameName(index);
        check(mkfifo(name.c_str(), 0600) == 0, "a frame dump pipe could not be created");
        descriptor = open(name.c_str(), O_RDWR);
        check(descriptor >= 0, "a frame dump pipe could not be opened");
    }

    ~HeldDump() { close(descriptor); }

    HeldDump(const HeldDump&) = delete;
    HeldDump& operator=(const HeldDump&) = delete;

    std::vector<char> Release() const {
        std::vector<char> dump(DumpBytes);
        for (std::size_t done = 0; done < dump.size();) {
            pollfd waiting{descriptor, POLLIN, 0};
            check(poll(&waiting, 1, 15000) == 1, "a queued frame dump was not written");
            const auto count = read(descriptor, dump.data() + done, dump.size() - done);
            check(count > 0, "a frame dump pipe could not be read");
            done += static_cast<std::size_t>(count);
        }
        return dump;
    }
};

std::vector<char> readDump(int index) {
    std::ifstream file(frameName(index), std::ios::binary);
    check(file.is_open(), "a frame dump is missing");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void checkDump(std::span<const char> dump, std::span<const std::byte> decoded) {
    check(dump.size() == DumpBytes, "a frame dump has the wrong size");
    std::uint32_t extent[2];
    std::memcpy(extent, dump.data() + 18, sizeof(extent));
    check(extent[0] == DumpWidth && extent[1] == static_cast<std::uint32_t>(-static_cast<std::int32_t>(DumpHeight)), "a frame dump has the wrong extent");
    for (std::uint32_t y = 0; y < DumpHeight; ++y) {
        for (std::uint32_t x = 0; x < DumpWidth; ++x) {
            const auto dumped = dump.data() + 54 + (static_cast<std::size_t>(y) * DumpWidth + x) * 4;
            const auto source = decoded.data() + (static_cast<std::size_t>(y) * Scale * Width + static_cast<std::size_t>(x) * Scale) * 4;
            check(std::memcmp(dumped, source, 4) == 0, "a frame dump pixel differs from the display buffer");
        }
    }
}

void testDumpQueue() {
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    constexpr auto tiledBytes = static_cast<std::size_t>((Width + 127) / 128) * ((Height + 127) / 128) * 65536;
    constexpr auto linearBytes = static_cast<std::size_t>(Width) * Height * 4;
    const auto makeStorage = [](std::size_t bytes, std::vector<std::byte>& allocation) {
        allocation.resize(bytes + 65535);
        const auto address = reinterpret_cast<std::uintptr_t>(allocation.data());
        auto* aligned = allocation.data() + ((65536u - (address & 65535u)) & 65535u);
        return std::span<std::byte>{aligned, bytes};
    };
    std::vector<std::byte> tiledAllocation;
    std::vector<std::byte> linearAllocation;
    const auto tiledStorage = makeStorage(tiledBytes, tiledAllocation);
    const auto linearStorage = makeStorage(linearBytes, linearAllocation);
    const AgcDriver::DisplayBuffer tiled{reinterpret_cast<std::uint64_t>(tiledStorage.data()), 0x8000000000000000ull, Width, Height};
    const AgcDriver::DisplayBuffer linear{reinterpret_cast<std::uint64_t>(linearStorage.data()), 0x8000000000000000ull, Width, Height, 1};
    check(AgcDriver::DisplayBufferSize(tiled) == tiledStorage.size(), "incorrect padded tiled display footprint");
    check(AgcDriver::DisplayBufferSize(linear) == linearStorage.size(), "incorrect linear display footprint");
    const auto fill = [](const AgcDriver::DisplayBuffer& display, std::span<std::byte> storage) {
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const auto offset = AgcDriver::DisplayBufferOffset(display, x, y);
                storage[offset] = static_cast<std::byte>(x & 255u);
                storage[offset + 1] = static_cast<std::byte>(y & 255u);
                storage[offset + 2] = static_cast<std::byte>((x >> 8u) | ((y >> 8u) << 4u));
                storage[offset + 3] = std::byte{255};
            }
        }
    };
    fill(tiled, tiledStorage);
    fill(linear, linearStorage);
    const auto tiledPixels = AgcDriver::ReadDisplayBuffer(tiled);
    const auto linearPixels = AgcDriver::ReadDisplayBuffer(linear);
    VideoOutBuffers tiledBuffer{tiledStorage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBuffers linearBuffer{linearStorage.data(), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 tiledAttribute{};
    VideoOutBufferAttribute2 linearAttribute{};
    sceVideoOutSetBufferAttribute2(&tiledAttribute, tiled.pixelFormat, 0, Width, Height, 0, 0, 0);
    sceVideoOutSetBufferAttribute2(&linearAttribute, linear.pixelFormat, 1, Width, Height, 0, 0, 0);
    check(sceVideoOutRegisterBuffers2(handle, 0, 0, &tiledBuffer, 1, &tiledAttribute, 0, nullptr) == 0, "tiled buffer registration failed");
    check(sceVideoOutRegisterBuffers2(handle, 1, 1, &linearBuffer, 1, &linearAttribute, 0, nullptr) == 0, "linear buffer registration failed");
    const HeldDump first(0);
    const HeldDump last(Frames - 1);
    std::vector<char> firstDump;
    std::thread reader;
    const auto started = std::chrono::steady_clock::now();
    if (WaitMode) reader = std::thread([&] {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        firstDump = first.Release();
    });
    std::exception_ptr failure;
    try {
        for (int frame = 0; frame < Frames; ++frame) {
            const int index = frame % 2;
            check(sceVideoOutSubmitFlip(handle, index, 1, frame) == 0, "the flip was not accepted");
            std::unique_lock lock(cfg->mutex);
            check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return (cfg->failure && cfg->flipStatus.flipPendingNum == 0) || cfg->flipStatus.count == static_cast<std::uint64_t>(frame) + 1; }), "the flip did not complete");
            if (cfg->failure) std::rethrow_exception(cfg->failure);
        }
    } catch (...) {
        failure = std::current_exception();
    }
    if (WaitMode) reader.join();
    else firstDump = first.Release();
    if (failure) std::rethrow_exception(failure);
    if (WaitMode) check(std::chrono::steady_clock::now() - started >= std::chrono::milliseconds(1500), "the presenter did not wait for the writer");
    checkDump(firstDump, tiledPixels);
    const auto lastDump = last.Release();
    checkDump(lastDump, linearPixels);
    checkDump(readDump(1), linearPixels);
    checkDump(readDump(2), tiledPixels);
    sceVideoOutUnregisterBuffers(handle, 0);
    sceVideoOutUnregisterBuffers(handle, 1);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
}

}

int main(int argc, char** argv) {
    WaitMode = argc > 1 && std::strcmp(argv[1], "wait") == 0;
    VideoOutTestEnvironment environment;
    try {
        if (WaitMode) check(Scale == 1, "the wait test needs APS5_DUMP_SCALE=1");
        testDumpQueue();
        std::puts(WaitMode ? "VideoOut frame dump wait test passed" : "VideoOut frame dump queue test passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        if (std::string(error.what()).find("Vulkan support") != std::string::npos && !std::getenv("ANYPS5_REQUIRE_DISPLAY")) {
            std::printf("skipped, no display or Vulkan device: %s\n", error.what());
            return 77;
        }
        return 1;
    }
}
