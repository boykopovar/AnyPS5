#include "prx/libSceVdecCore/include/Api.hpp"
#include "prx/libSceVdecCore/include/Decoder.hpp"
#include "H264Fixture.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

constexpr int NoPicture = static_cast<int>(0x80C00010);
constexpr std::uint32_t CodecAvc = 0;
constexpr std::uint32_t DecoderVdec2 = 2;
constexpr std::size_t ThreadCount = 4;
constexpr std::size_t Rounds = 40;
constexpr std::int64_t PtsBase = 1000;
constexpr std::int64_t DtsBase = 2000;
constexpr std::uint64_t UserBase = 3000;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "VdecCore concurrent output: %s\n", message);
        std::abort();
    }
}

VdecCoreConfigInfo Config() {
    VdecCoreConfigInfo config{};
    config.decoderType = DecoderVdec2;
    config.codecType = CodecAvc;
    config.maxFrameWidth = static_cast<std::int32_t>(H264Fixture::Width);
    config.maxFrameHeight = static_cast<std::int32_t>(H264Fixture::Height);
    config.horizontalAlign = 256;
    config.maxDpbFrameCount = 0;
    return config;
}

const std::vector<std::vector<std::uint8_t>>& Units() {
    static const std::vector<std::vector<std::uint8_t>> units = H264Fixture::accessUnits(H264Fixture::Stream.data(), H264Fixture::Stream.size(), false);
    Require(units.size() == H264Fixture::PictureHashes.size(), "the fixture's access units");
    return units;
}

void FeedAndFlush(void* decoder) {
    for (std::size_t unit = 0; unit < Units().size(); ++unit) {
        const auto& data = Units()[unit];
        const auto offset = static_cast<std::int64_t>(unit);
        const VdecCoreInputData input{data.data(), data.size(), PtsBase + offset, DtsBase + offset, UserBase + unit, 0, 0};
        std::array<std::uint32_t, 2> status{};
        Require(sceVdecCoreSetDecodeInput(decoder, &input, nullptr, status.data()) == 0, "set an access unit as decode input");
    }
    std::uint32_t queued = 0;
    Require(sceVdecCoreFlushDecodeOutput(decoder, &queued, nullptr) == 0, "flush the decode output");
    Require(queued == Units().size(), "a flush holds every picture for the fallback");
}

void RunRound() {
    const VdecCoreConfigInfo config = Config();
    void* decoder = nullptr;
    Require(sceVdecCoreCreateDecoder(&config, nullptr, nullptr, &decoder) == 0 && decoder != nullptr, "create an AVC decoder");
    FeedAndFlush(decoder);

    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::array<std::vector<std::size_t>, ThreadCount> seen;
    std::vector<std::thread> threads;
    for (std::size_t index = 0; index < ThreadCount; ++index) {
        threads.emplace_back([&, index] {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            try {
                while (true) {
                    VdecCoreOutputInfo output{};
                    const int result = sceVdecCoreGetDecodeOutput(decoder, nullptr, 0, &output, nullptr);
                    if (result == NoPicture) break;
                    if (result != 0 || output.pictureCount != 1 || output.pictures[0].frameBuffer == nullptr) {
                        failed.store(true);
                        break;
                    }
                    const std::uint64_t unit = output.pictures[0].pts - static_cast<std::uint64_t>(PtsBase);
                    if (unit >= Units().size() || output.pictures[0].userData != UserBase + unit) {
                        failed.store(true);
                        break;
                    }
                    seen[index].push_back(static_cast<std::size_t>(unit));
                }
            } catch (...) {
                failed.store(true);
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (auto& thread : threads) thread.join();

    Require(!failed.load(), "a concurrent call returned an inconsistent picture or failed");
    std::array<int, 6> count{};
    for (const auto& list : seen) {
        for (const std::size_t unit : list) ++count[unit];
    }
    for (const int times : count) Require(times == 1, "each picture is returned exactly once across the threads");
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete the decoder");
}

}

int main() {
    Units();
    for (std::size_t round = 0; round < Rounds; ++round) RunRound();
    return 0;
}
