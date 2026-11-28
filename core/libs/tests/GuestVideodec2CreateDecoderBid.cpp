#include "prx/libSceVideodec2/include/Api.hpp"
#include "H264Fixture.hpp"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

using namespace SceVideodec2;

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
static std::string Failure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    throw std::runtime_error("invalid argument did not throw");
}

int main() {
    try {
        constexpr int ErrorResourceType = static_cast<int>(0x811D0203u);
        const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 0, 1, 1, 0, H264Fixture::Width, H264Fixture::Height, 4, 1};
        DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
        Require(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "query memory info failed");

        std::uint64_t handle = 0;
        Require(Failure([&] { sceVideodec2CreateDecoderBid(1, nullptr, &memory, &handle); }) == "Videodec2: invalid argument pointer", "null config");
        Require(Failure([&] { sceVideodec2CreateDecoderBid(1, &config, nullptr, &handle); }) == "Videodec2: invalid argument pointer", "null memory");
        Require(Failure([&] { sceVideodec2CreateDecoderBid(1, &config, &memory, nullptr); }) == "Videodec2: invalid argument pointer", "null handle");

        DecoderConfigInfo computeConfig = config;
        computeConfig.resourceType = 1;
        std::uint64_t computeHandle = 0x12345678ull;
        Require(sceVideodec2CreateDecoderBid(1, &computeConfig, &memory, &computeHandle) == ErrorResourceType, "resource type 1 rejected");
        Require(computeHandle == 0x12345678ull, "handle unchanged on resource type error");

        Require(Failure([&] { sceVideodec2CreateDecoderBid(0, &config, &memory, &handle); }) == "sceVideodec2CreateDecoderBid not implemented", "bid 0 unsupported");
        Require(Failure([&] { sceVideodec2CreateDecoderBid(2, &config, &memory, &handle); }) == "sceVideodec2CreateDecoderBid not implemented", "bid 2 unsupported");

        Require(sceVideodec2CreateDecoderBid(1, &config, &memory, &handle) == 0 && handle != 0, "create decoder bid 1");

        std::vector<std::uint8_t> frameStorage(memory.maxFrameBufferSize);
        FrameBuffer frame{sizeof(FrameBuffer), frameStorage.data(), frameStorage.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        const InputData emptyInput{sizeof(InputData), nullptr, 0, 0, 0, 0};
        Require(sceVideodec2Decode_nid_postfix(handle, &emptyInput, &frame, &output) != 0, "decode rejected empty input");

        Require(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete decoder");
        Require(Failure([&] { sceVideodec2DeleteDecoder_nid_postfix(handle); }) == "Videodec2: invalid decoder handle", "double delete rejected");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
