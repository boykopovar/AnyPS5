#include "prx/libSceVoice/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

namespace {

constexpr auto NotInitialized = static_cast<int>(0x804e0801u);
constexpr auto PortWithoutDevice = static_cast<int>(0x804e0803u);
constexpr auto InvalidPort = static_cast<int>(0x804e0804u);
constexpr auto InvalidArgument = static_cast<int>(0x804e0805u);
constexpr auto MemorySizeVersion = static_cast<int>(0x804e080fu);
constexpr std::uint32_t Untouched = 0x5a5a5a5au;

using MemorySizeFn = int (APS5_VABI*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t*);
using CustomerDataFn = int (APS5_VABI*)(std::uint32_t, const void*, std::uint32_t, float*);
using QaRecordingFn = int (APS5_VABI*)();
using AdjustmentFn = void (APS5_VABI*)(float, float);
using PvtAdjustmentFn = void (APS5_VABI*)(float, float, float, float);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Voice extras: %s\n", message);
    std::abort();
}

template <typename Function, typename Export>
Function As(Export function) {
    return reinterpret_cast<Function>(function);
}

}

int main() {
    const auto memorySize = As<MemorySizeFn>(&sceVoiceGetMemorySize);
    const auto customerData = As<CustomerDataFn>(&sceVoiceDeviceInCustomerData);
    const auto qaRecording = As<QaRecordingFn>(&sceVoiceQAEnableRecording);
    const auto adjustment = As<AdjustmentFn>(&sceVoiceVADAdjustment);
    const auto pvtAdjustment = As<PvtAdjustmentFn>(&sceVoiceVADPVTAdjustment);
    unsigned char data[64] = {};
    float gain = 1.0f;
    std::uint32_t size = Untouched;

    adjustment(0.5f, 2.0f);
    pvtAdjustment(0.25f, 0.5f, 1.0f, 2.0f);
    Require(qaRecording() == NotInitialized, "QA recording was enabled before sceVoiceInit");
    Require(customerData(0, data, sizeof(data), &gain) == NotInitialized && gain == 1.0f, "customer data was taken before sceVoiceInit");
    Require(customerData(0, nullptr, 0, nullptr) == NotInitialized, "customer data checked its arguments before initialization");
    Require(memorySize(2, 0, 0, 0, 0, 0, 0, 0, 0, &size) == NotInitialized && size == Untouched, "the memory size was answered before sceVoiceInit");
    Require(memorySize(2, 0, 0, 0, 0, 0, 0, 0, 0, nullptr) == NotInitialized, "the memory size checked its output before initialization");

    VoiceInitParam init{};
    init.app_type = 4;
    Require(sceVoiceInit(&init, 100) == 0, "sceVoiceInit failed");
    adjustment(1.0f, 0.0f);
    pvtAdjustment(0.0f, 0.0f, 0.0f, 0.0f);
    Require(qaRecording() == 0, "QA recording was not enabled after sceVoiceInit");

    Require(customerData(0, nullptr, 0, &gain) == InvalidArgument, "customer data took null data");
    Require(customerData(0, data, 0, nullptr) == InvalidArgument, "customer data took a null gain");
    for (const float bad : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), -1.0f, -0.5f}) {
        gain = bad;
        Require(customerData(0, data, sizeof(data), &gain) == InvalidArgument, "customer data took an infinite, NaN or negative gain before looking up the port");
    }
    gain = 0.0f;
    Require(customerData(5, data, 0, &gain) == InvalidPort, "customer data took an unknown port");

    VoicePortParam param{};
    param.port_type = 0;
    param.volume = 1.0f;
    std::uint32_t port = 0;
    Require(sceVoiceCreatePort(&port, &param) == 0, "creating an input device port failed");
    gain = 0.0f;
    Require(customerData(port, data, 0, &gain) == PortWithoutDevice && gain == 0.0f, "customer data on a port without a device did not fail");
    gain = 2.0f;
    Require(customerData(port, data, sizeof(data), &gain) == PortWithoutDevice && gain == 2.0f, "customer data on a port without a device changed the gain");

    Require(memorySize(2, 0, 0, 0, 0, 0, 0, 0, 0, nullptr) == InvalidArgument, "the memory size took a null output");
    size = Untouched;
    Require(memorySize(2, 0, 0, 0, 0, 0, 0, 0, 0, &size) == MemorySizeVersion && size == 0, "the memory size did not refuse a library initialized with version 100");

    std::puts("Voice extras tests passed");
    return 0;
}
