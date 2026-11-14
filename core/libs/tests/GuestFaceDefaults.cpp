#include "prx/libSceFace/include/Api.hpp"
#include "prx/libSceFaceTracker/include/Api.hpp"
#include "SceTypes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

constexpr int kFaceErrorParameter = static_cast<int>(0x808b0002u);
constexpr int kTrackerErrorInvalid = static_cast<int>(0x812a0001u);

}

int main() {
    alignas(16) std::uint32_t detection[0x40];
    std::memset(detection, 0xaa, sizeof(detection));
    Require(sceFaceDetectionGetDefaultParam(nullptr) == kFaceErrorParameter, "null detection parameter was accepted");
    Require(sceFaceDetectionGetDefaultParam(detection) == 0, "the detection defaults failed");
    const std::uint32_t block[14] = {56u, 0x3f800000u, 0x3f574bc7u, 13200u, 0u, 0u, 0u, 2u, 2u, 0u, 0u, 0x3f000000u, 1u, 0u};
    for (int word = 0; word < 14; ++word) Require(detection[word] == block[word], "the detection defaults block differs from the module bytes");
    Require(detection[14] == 0xaaaaaaaau && detection[15] == 0xaaaaaaaau, "the detection defaults wrote past the 0x38-byte block");

    alignas(16) std::uint32_t config[0x40];
    std::memset(config, 0xaa, sizeof(config));
    Require(sceFaceTrackerGetDefaultConfig(nullptr) == kTrackerErrorInvalid, "null tracker config was accepted");
    Require(sceFaceTrackerGetDefaultConfig(config) == 0, "the tracker defaults failed");
    Require(config[0x00 / 4] == detection[0x08 / 4] && config[0x04 / 4] == detection[0x10 / 4] && config[0x08 / 4] == detection[0x0c / 4],
        "the tracker first words are not the remapped detection fields");
    Require(config[0x0c / 4] == 0 && config[0x10 / 4] == 0 && config[0x1c / 4] == 0 && config[0x20 / 4] == 0 && config[0x2c / 4] == 0,
        "the tracker remap left nonzero fields where the module writes zero");
    Require(config[0x14 / 4] == detection[0x1c / 4] && config[0x18 / 4] == detection[0x20 / 4], "the tracker did not remap the pair of twos");
    Require(config[0x24 / 4] == detection[0x2c / 4] && config[0x28 / 4] == detection[0x30 / 4], "the tracker did not remap the half and one");
    const std::uint32_t tail[15] = {0x3f574bc7u, 0x40000000u, 0x40000000u, 2u, 2u, 0x3e99999au, 4u,
                                    0x42b40000u, 5u, 0x42480000u, 10u, 0xfu, 0x42500000u, 30u, 15u};
    for (int word = 0; word < 6; ++word) Require(config[0x38 / 4 + word] == tail[word], "the tracker sub-fill at +0x38 differs");
    Require(config[0x50 / 4] == tail[6], "the tracker constant at +0x50 differs");
    for (int word = 0; word < 4; ++word) Require(config[0x54 / 4 + word] == tail[7 + word], "the tracker sub-fill at +0x54 differs");
    Require(reinterpret_cast<std::uint8_t *>(config)[0x64] == 1, "the tracker byte at +0x64 is not one");
    Require(config[0x68 / 4] == 0xfu, "the tracker inline word at +0x68 differs");
    Require(config[0x6c / 4] == 0x42500000u, "the tracker float at +0x6c differs");
    Require(config[0x70 / 4] == 30u && config[0x74 / 4] == 15u, "the tracker inline pair at +0x70 differs");
    Require(config[0x78 / 4] == 5u, "the tracker word at +0x78 differs");
    Require(reinterpret_cast<std::uint8_t *>(config)[0x7c] == 1 && reinterpret_cast<std::uint8_t *>(config)[0x7d] == 1, "the tracker bytes at +0x7c differ");

    std::puts("Face defaults tests passed");
    return 0;
}
