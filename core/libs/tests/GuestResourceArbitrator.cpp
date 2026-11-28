#include "prx/libSceResourceArbitrator/libSceResourceArbitrator.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

bool AllEqual(const std::array<std::uint8_t, 128>& buffer, std::uint8_t expected) {
    for (std::uint8_t byte : buffer) {
        if (byte != expected) {
            return false;
        }
    }
    return true;
}

void WriteFeatureEntry(std::array<unsigned char, 0x100>& list, std::size_t index, const char* name) {
    unsigned char* dst = list.data() + 0x18u + index * 0x20u;
    std::memset(dst, 0, 0x20u);
    const std::size_t len = std::strlen(name);
    const std::size_t copyLen = len < 0x20u ? len : 0x20u;
    std::memcpy(dst, name, copyLen);
}

void SetFeatureCount(std::array<unsigned char, 0x100>& list, std::uint32_t count) {
    std::memcpy(list.data() + 4u, &count, sizeof(count));
}

} // namespace

int main() {
    constexpr int kErrorUnavailable = static_cast<int>(0x819b0001u);
    constexpr int kErrorNotInitialized = static_cast<int>(0x819b0002u);
    constexpr int kErrorInvalidArg = static_cast<int>(0x819b0003u);

    Require(sceResourceArbitratorSetCallback(1u, nullptr, nullptr) == kErrorNotInitialized);
    Require(sceResourceArbitratorClearCallback(1u) == kErrorNotInitialized);
    Require(sceResourceArbitratorChangeState(1u, "game", nullptr, 0u) == kErrorNotInitialized);
    Require(sceResourceArbitratorIsExecutable(1u, "game") == kErrorNotInitialized);
    Require(sceResourceArbitratorPostTelemetry(1u, 2u, "metric", 6u) == kErrorNotInitialized);
    Require(sceResourceArbitratorSendDebugFunction(0u) == kErrorNotInitialized);
    Require(libSceResourceArbitratorUnknown_jzKwHzhEBIQ_nid_no_patch(1u) == kErrorNotInitialized);
    Require(libSceResourceArbitratorUnknown_3xRoVp7fPPY_nid_no_patch() == kErrorNotInitialized);

    std::array<std::uint8_t, 128> outBuffer{};
    outBuffer.fill(0xA5u);
    Require(sceResourceArbitratorGetFeatureIdList(outBuffer.data()) == kErrorUnavailable);
    Require(AllEqual(outBuffer, 0xA5u));
    Require(libSceResourceArbitratorUnknown_OM6SXhDZdiU_nid_no_patch(1u, 2u) == kErrorUnavailable);
    Require(libSceResourceArbitratorUnknown_ru9qkt9UCLQ_nid_no_patch(1u, outBuffer.data()) == kErrorUnavailable);
    Require(AllEqual(outBuffer, 0xA5u));

    std::int32_t outIndex = -99;
    std::int32_t outValue = -77;
    Require(libSceResourceArbitratorUnknown_rMJnXXj6GIo_nid_no_patch(-1, &outIndex, &outValue) == kErrorInvalidArg);
    Require(outIndex == -99 && outValue == -77);
    Require(libSceResourceArbitratorUnknown_rMJnXXj6GIo_nid_no_patch(0, &outIndex, &outValue) == kErrorUnavailable);
    Require(outIndex == -99 && outValue == -77);

    Require(libSceResourceArbitratorUnknown_TYiIrKTzQ8s_nid_no_patch(0u) == 0);
    Require(libSceResourceArbitratorUnknown_TYiIrKTzQ8s_nid_no_patch(1u) == 0);

    std::array<unsigned char, 0x100> featureList{};
    featureList.fill(0);
    WriteFeatureEntry(featureList, 0u, "audio");
    WriteFeatureEntry(featureList, 1u, "video");
    WriteFeatureEntry(featureList, 2u, "third");
    WriteFeatureEntry(featureList, 4u, "fifth");
    WriteFeatureEntry(featureList, 5u, "sixth");

    Require(libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(nullptr, "audio") == 0);
    Require(libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(featureList.data(), nullptr) == 0);

    SetFeatureCount(featureList, 0u);
    Require(libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(featureList.data(), "audio") == 0);

    SetFeatureCount(featureList, 2u);
    Require(libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(featureList.data(), "audio") == 1);
    Require(libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(featureList.data(), "video") == 1);
    Require(libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(featureList.data(), "third") == 0);

    SetFeatureCount(featureList, 10u);
    Require(libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(featureList.data(), "fifth") == 1);
    Require(libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(featureList.data(), "sixth") == 0);

    std::array<unsigned char, 0x21> fullLen{};
    fullLen.fill('x');
    fullLen[0x20] = '\0';
    std::memset(featureList.data() + 0x18u, 'x', 0x20u);
    Require(libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(
                featureList.data(), reinterpret_cast<const char*>(fullLen.data())) == 1);
    fullLen[0x1fu] = 'y';
    Require(libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(
                featureList.data(), reinterpret_cast<const char*>(fullLen.data())) == 0);

    return 0;
}
