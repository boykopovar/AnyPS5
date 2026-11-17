#include "prx/libSceVideodec2/include/Api.hpp"
#include "H264Fixture.hpp"
#include "HevcFixture.hpp"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace SceVideodec2;

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static auto Fields(const DecoderMemoryInfo& memory) {
    return std::tie(memory.thisSize, memory.cpuMemorySize, memory.cpuMemory, memory.gpuMemorySize, memory.gpuMemory,
        memory.cpuGpuMemorySize, memory.cpuGpuMemory, memory.maxFrameBufferSize, memory.frameBufferAlignment);
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
        const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, 974921, 1, 0, H264Fixture::Width, H264Fixture::Height, 4, 1};
        DecoderMemoryInfo generic{sizeof(DecoderMemoryInfo)}, alias{sizeof(DecoderMemoryInfo)};
        const auto expected = sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &generic);
        Require(sceVideodec2QueryHevcDecoderMemoryInfo(&config, &alias) == expected && expected == 0, "memory query result differs");
        Require(Fields(alias) == Fields(generic), "memory query fields differ");
        Require(Failure([&] { sceVideodec2QueryHevcDecoderMemoryInfo(nullptr, &alias); }) ==
            Failure([&] { sceVideodec2QueryDecoderMemoryInfo_nid_postfix(nullptr, &generic); }), "query argument error differs");
        std::uint64_t handle = 0, reference = 0;
        Require(Failure([&] { sceVideodec2CreateHevcDecoder(&config, nullptr, &handle); }) ==
            Failure([&] { sceVideodec2CreateDecoder_nid_postfix(&config, nullptr, &reference); }), "create argument error differs");
        Require(sceVideodec2CreateHevcDecoder(&config, &alias, &handle) == 0 && handle != 0, "alias creation failed");
        Require(sceVideodec2CreateDecoder_nid_postfix(&config, &generic, &reference) == 0, "generic creation failed");
        std::vector<std::uint8_t> storage;
        const auto buffer = HevcFixture::AlignedFrameStorage(storage, alias.maxFrameBufferSize, alias.frameBufferAlignment);
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        const InputData empty{sizeof(InputData), nullptr, 0, 0, 0, 0};
        const auto error = sceVideodec2Decode_nid_postfix(reference, &empty, &frame, &output);
        Require(error != 0 && sceVideodec2Decode_nid_postfix(handle, &empty, &frame, &output) == error, "decoder error code differs");
        Require(sceVideodec2DeleteDecoder_nid_postfix(reference) == 0, "generic deletion failed");
        std::size_t pictures = 0;
        const auto take = [&] {
            Require(frame.isAccepted == output.isValid, "frame acceptance differs");
            if (!output.isValid) return false;
            Require(pictures < HevcFixture::HevcHashes.size(), "too many pictures");
            Require(output.codecType == config.codecType && !output.isErrorFrame && output.frameWidth == H264Fixture::Width &&
                output.frameHeight == H264Fixture::Height && output.framePitch == 256 && output.frameBuffer == buffer.data(), "picture geometry differs");
            Require(H264Fixture::hashNv12(buffer.data(), output.framePitch, output.frameHeight) == HevcFixture::HevcHashes[pictures], "picture hash differs");
            ++pictures;
            return true;
        };
        const auto units = HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size());
        for (const auto& unit : units) {
            const InputData input{sizeof(InputData), unit.data(), unit.size(), 0, 0, 0};
            Require(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
            take();
        }
        do {
            Require(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        } while (take());
        Require(pictures == HevcFixture::HevcHashes.size(), "missing pictures");
        Require(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "alias deletion failed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
