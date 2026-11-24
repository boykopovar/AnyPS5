#include "prx/libSceVideodec2/include/Api.hpp"
#include "H264Fixture.hpp"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

using namespace SceVideodec2;

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        constexpr int ErrorDecoderInstance = static_cast<int>(0x811D0103u);
        constexpr int ErrorInvalidPointer = static_cast<int>(0x811D0102u);
        constexpr int ErrorMemorySize = static_cast<int>(0x811D0104u);

        std::vector<std::uint8_t> buffer(4096);

        Require(sceVideodec2MapMemory(0, buffer.data(), buffer.size()) == ErrorDecoderInstance, "zero handle rejected");
        Require(sceVideodec2MapMemory(0xDEADBEEFull, buffer.data(), buffer.size()) == ErrorDecoderInstance, "nonexistent handle rejected");

        const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 0, 1, 1, 0, H264Fixture::Width, H264Fixture::Height, 4, 1};
        DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
        Require(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "query memory info failed");

        std::uint64_t handle = 0;
        Require(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0 && handle != 0, "create decoder failed");

        Require(sceVideodec2MapMemory(handle, nullptr, buffer.size()) == ErrorInvalidPointer, "null memory rejected");
        Require(sceVideodec2MapMemory(handle, buffer.data(), 0) == ErrorMemorySize, "zero size rejected");
        Require(sceVideodec2MapMemory(handle, buffer.data(), buffer.size()) == 0, "valid map memory succeeded");

        Require(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete decoder failed");
        Require(sceVideodec2MapMemory(handle, buffer.data(), buffer.size()) == ErrorDecoderInstance, "deleted handle rejected");

        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
