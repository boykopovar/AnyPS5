#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/include/InventoryExports.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr int ErrorInvalidParam = static_cast<int>(0x80268001);
constexpr int ErrorInvalidArgument = static_cast<int>(0x80260502);
constexpr int ErrorInvalidHandle = static_cast<int>(0x80260503);
constexpr int ErrorInvalidGrains = static_cast<int>(0x8026800d);
constexpr int ErrorUnsupportedFormat = static_cast<int>(0x8026800e);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestSet3DLatencyBounds() {
    Require(sceAudioOut2Set3DLatency(0, 0) == 0);
    Require(sceAudioOut2Set3DLatency(0, 1) == 0);
    Require(sceAudioOut2Set3DLatency(0, 2) == 0);

    Require(sceAudioOut2Set3DLatency(0xFF, 1) == 0);
    Require(sceAudioOut2Set3DLatency(0xFF, 2) == 0);

    Require(sceAudioOut2Set3DLatency(0xFF, 0) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0x10000000, 0) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0x10000000, 1) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0x10000000, 2) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(-1, 0) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(-1, 1) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(1, 1) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(2, 1) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0xFE, 1) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0x100, 1) == ErrorInvalidParam);

    Require(sceAudioOut2Set3DLatency(0, 3) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0, 4) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0, 100) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0, 0xFFFFFFFFu) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0xFF, 3) == ErrorInvalidParam);
    Require(sceAudioOut2Set3DLatency(0xFF, 0xFFFFFFFFu) == ErrorInvalidParam);
}

void TestLowLatencyLifecycle() {
    Require(sceAudioOut2LoInit() == 0);
    Require(sceAudioOut2LoInit() == 0);
    Require(sceAudioOut2LoTerminate() == 0);
    Require(sceAudioOut2LoTerminate() == 0);
    Require(sceAudioOut2LoInit() == 0);
}

void TestLowLatencyContextQueryMemoryBounds() {
    std::size_t memorySize = 0;
    Require(sceAudioOut2LoContextQueryMemory(nullptr, &memorySize) == ErrorInvalidParam);
    Require(sceAudioOut2LoContextQueryMemory(nullptr, nullptr) == ErrorInvalidParam);

    const std::uint32_t exParamsValid[2] = {4, 2};
    Require(sceAudioOut2LoContextQueryMemoryEx(nullptr, exParamsValid, &memorySize) == ErrorInvalidParam);
    Require(sceAudioOut2LoContextQueryMemoryEx(nullptr, nullptr, &memorySize) == ErrorInvalidParam);

    AudioOut2ContextParam params{};
    Require(sceAudioOut2ContextResetParam(&params) == 0);
    params.num_grains = 256;
    params.max_ports = 16;
    params.max_object_ports = 32;
    params.guarantee_object_ports = 16;
    params.queue_depth = 8;

    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == 0);
    Require(memorySize > 0);
    Require(sceAudioOut2LoContextQueryMemory(&params, nullptr) == 0);

    params.max_object_ports = 0x80;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == 0);
    params.max_object_ports = 0x81;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidParam);
    params.max_object_ports = 0x100;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidParam);
    params.max_object_ports = 32;

    params.max_ports = 0x20;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == 0);
    params.max_ports = 0x21;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidParam);
    params.max_ports = 0x40;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidParam);
    params.max_ports = 16;

    params.max_object_ports = 10;
    params.guarantee_object_ports = 11;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidParam);
    params.guarantee_object_ports = 10;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == 0);
    params.guarantee_object_ports = 0;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == 0);
    params.max_object_ports = 32;
    params.guarantee_object_ports = 16;

    params.num_grains = 0;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidGrains);
    params.num_grains = 1;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidGrains);
    params.num_grains = 128;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidGrains);
    params.num_grains = 255;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidGrains);
    params.num_grains = 257;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidGrains);
    params.num_grains = 384;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidGrains);
    params.num_grains = 511;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidGrains);
    params.num_grains = 513;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidGrains);
    params.num_grains = 0x801;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidGrains);
    params.num_grains = 0x900;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidGrains);
    params.num_grains = 0x1000;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidGrains);

    for (std::uint32_t grains = 0x100; grains <= 0x800; grains += 0x100) {
        params.num_grains = grains;
        Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == 0);
    }
    params.num_grains = 256;

    params.queue_depth = 0;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == 0);
    params.queue_depth = 16;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == 0);
    params.queue_depth = 17;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidParam);
    params.queue_depth = 32;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidParam);
    params.queue_depth = 64;
    Require(sceAudioOut2LoContextQueryMemory(&params, &memorySize) == ErrorInvalidParam);
    params.queue_depth = 8;

    std::uint32_t exParams[2] = {0x15, 0};
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, exParams, &memorySize) == 0);
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, exParams, nullptr) == 0);

    exParams[0] = 0x16;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, exParams, &memorySize) == ErrorInvalidParam);
    exParams[0] = 0x50;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, exParams, &memorySize) == ErrorInvalidParam);
    exParams[0] = 5;

    params.queue_depth = 17;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, exParams, &memorySize) == 0);
    params.queue_depth = 0x40;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, exParams, &memorySize) == 0);
    params.queue_depth = 0x41;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, exParams, &memorySize) == ErrorInvalidParam);
    params.queue_depth = 100;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, exParams, &memorySize) == ErrorInvalidParam);
    params.queue_depth = 8;

    auto computeExpected = [](std::uint32_t maxPorts, std::uint32_t extraPorts, std::uint32_t maxObjPorts, bool hasEx) {
        auto slotWords = [](std::uint32_t n) { return 0xc + ((n + 0x1f) >> 5) * 4; };
        return static_cast<std::size_t>(maxPorts + extraPorts + maxObjPorts) * 0xb60
            + static_cast<std::size_t>(slotWords(maxPorts) + slotWords(maxObjPorts))
            + (hasEx ? static_cast<std::size_t>(slotWords(extraPorts)) : 0)
            + ((static_cast<std::size_t>(maxObjPorts) * 24 + 0x7f) & ~static_cast<std::size_t>(0x7f));
    };

    std::size_t queriedBase = 0;
    Require(sceAudioOut2LoContextQueryMemory(&params, &queriedBase) == 0);
    Require(queriedBase == computeExpected(params.max_ports, 0, params.max_object_ports, false));

    std::size_t queriedEx = 0;
    exParams[0] = 7;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, exParams, &queriedEx) == 0);
    Require(queriedEx == computeExpected(params.max_ports, 7, params.max_object_ports, true));
}

void TestLowLatencyContextCreateBounds(AudioOut2ContextHandle& outContext, std::vector<std::byte>& outBuffer) {
    AudioOut2ContextParam params{};
    Require(sceAudioOut2ContextResetParam(&params) == 0);
    params.num_grains = 256;
    params.max_ports = 4;
    params.max_object_ports = 8;
    params.guarantee_object_ports = 4;
    params.queue_depth = 4;

    std::size_t required = 0;
    Require(sceAudioOut2LoContextQueryMemory(&params, &required) == 0);
    Require(required > 0);

    outBuffer.resize(required);
    AudioOut2ContextHandle ctx = 0;

    Require(sceAudioOut2LoContextCreate(nullptr, outBuffer.data(), required, &ctx) == ErrorInvalidParam);
    Require(ctx == static_cast<AudioOut2ContextHandle>(-1));

    Require(sceAudioOut2LoContextCreate(&params, nullptr, required, &ctx) == ErrorInvalidParam);
    Require(ctx == static_cast<AudioOut2ContextHandle>(-1));

    Require(sceAudioOut2LoContextCreate(&params, outBuffer.data(), required, nullptr) == ErrorInvalidParam);

    Require(sceAudioOut2LoContextCreate(&params, outBuffer.data(), required - 1, &ctx) == ErrorInvalidParam);
    Require(ctx == static_cast<AudioOut2ContextHandle>(-1));

    Require(sceAudioOut2LoContextCreate(&params, outBuffer.data(), 0, &ctx) == ErrorInvalidParam);
    Require(ctx == static_cast<AudioOut2ContextHandle>(-1));

    params.num_grains = 200;
    Require(sceAudioOut2LoContextCreate(&params, outBuffer.data(), required, &ctx) == ErrorInvalidGrains);
    params.num_grains = 256;

    std::uint32_t exParams[2] = {2, 9};
    std::size_t exRequired = 0;
    Require(sceAudioOut2LoContextQueryMemoryEx(&params, exParams, &exRequired) == 0);
    std::vector<std::byte> exBuffer(exRequired);

    Require(sceAudioOut2LoContextCreateEx(&params, exParams, exBuffer.data(), exRequired, &ctx) == ErrorInvalidParam);
    Require(ctx == static_cast<AudioOut2ContextHandle>(-1));

    exParams[1] = 8;
    Require(sceAudioOut2LoContextCreateEx(&params, exParams, exBuffer.data(), exRequired, &ctx) == 0);
    Require(ctx != 0 && ctx != static_cast<AudioOut2ContextHandle>(-1));
    Require(sceAudioOut2LoContextDestroy(ctx) == 0);

    Require(sceAudioOut2LoContextCreate(&params, outBuffer.data(), required, &ctx) == 0);
    Require(ctx != 0 && ctx != static_cast<AudioOut2ContextHandle>(-1));
    outContext = ctx;
}

void TestLowLatencyContextOperationsBounds(AudioOut2ContextHandle ctx) {
    constexpr AudioOut2ContextHandle invalidCtx = 0;
    constexpr AudioOut2ContextHandle nonExistentCtx = 0x9876;

    Require(sceAudioOut2LoContextDestroy(invalidCtx) == ErrorInvalidHandle);
    Require(sceAudioOut2LoContextDestroy(nonExistentCtx) == ErrorInvalidHandle);

    Require(sceAudioOut2LoContextAdvance(invalidCtx) == ErrorInvalidHandle);
    Require(sceAudioOut2LoContextAdvance(nonExistentCtx) == ErrorInvalidHandle);

    Require(sceAudioOut2LoContextPush(invalidCtx, 0) == ErrorInvalidHandle);
    Require(sceAudioOut2LoContextPush(nonExistentCtx, 1) == ErrorInvalidHandle);

    std::uint32_t qLevel = 99;
    std::uint32_t qAvail = 99;
    Require(sceAudioOut2LoContextGetQueueLevel(invalidCtx, &qLevel, &qAvail) == ErrorInvalidHandle);
    Require(sceAudioOut2LoContextGetQueueLevel(nonExistentCtx, &qLevel, &qAvail) == ErrorInvalidHandle);

    AudioOut2Attribute attr{};
    Require(sceAudioOut2LoContextSetAttributes(invalidCtx, &attr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoContextSetAttributes(nonExistentCtx, &attr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoContextSetAttributes(ctx, nullptr, 1) == ErrorInvalidArgument);
    Require(sceAudioOut2LoContextSetAttributes(ctx, nullptr, 0) == 0);
    Require(sceAudioOut2LoContextSetAttributes(ctx, &attr, 0) == 0);

    Require(sceAudioOut2LoContextGetAttributes(invalidCtx, &attr, 1) == ErrorInvalidParam);
    Require(sceAudioOut2LoContextGetAttributes(nonExistentCtx, &attr, 1) == ErrorInvalidParam);
    Require(sceAudioOut2LoContextGetAttributes(ctx, nullptr, 1) == ErrorInvalidParam);
    Require(sceAudioOut2LoContextGetAttributes(ctx, nullptr, 0) == 0);

    attr.attribute_id = 0;
    Require(sceAudioOut2LoContextGetAttributes(ctx, &attr, 1) == ErrorInvalidParam);
    attr.attribute_id = 1;
    Require(sceAudioOut2LoContextGetAttributes(ctx, &attr, 1) == ErrorInvalidParam);
    attr.attribute_id = 0x1d;
    Require(sceAudioOut2LoContextGetAttributes(ctx, &attr, 1) == ErrorInvalidParam);
    attr.attribute_id = 0x20;
    Require(sceAudioOut2LoContextGetAttributes(ctx, &attr, 1) == ErrorInvalidParam);

    bool threw1e = false;
    try {
        attr.attribute_id = 0x1e;
        sceAudioOut2LoContextGetAttributes(ctx, &attr, 1);
    } catch (const std::runtime_error&) {
        threw1e = true;
    }
    Require(threw1e);

    bool threw1f = false;
    try {
        attr.attribute_id = 0x1f;
        sceAudioOut2LoContextGetAttributes(ctx, &attr, 1);
    } catch (const std::runtime_error&) {
        threw1f = true;
    }
    Require(threw1f);

    Require(sceAudioOut2LoContextGetQueueLevel(ctx, nullptr, nullptr) == 0);
    Require(sceAudioOut2LoContextGetQueueLevel(ctx, &qLevel, nullptr) == 0);
    Require(sceAudioOut2LoContextGetQueueLevel(ctx, nullptr, &qAvail) == 0);
    Require(sceAudioOut2LoContextGetQueueLevel(ctx, &qLevel, &qAvail) == 0);

    Require(sceAudioOut2LoContextAdvance(ctx) == 0);
    Require(sceAudioOut2LoContextPush(ctx, 0) == 0);
}

void TestLowLatencyPortLifecycleAndAttributeBounds(AudioOut2ContextHandle ctx) {
    constexpr AudioOut2ContextHandle invalidCtx = 0;
    constexpr AudioOut2ContextHandle nonExistentCtx = 0x7654;

    AudioOut2PortParam portParam{};
    portParam.port_type = 0;
    portParam.data_format = 2u << 8;
    portParam.sampling_freq = 48000;

    AudioOut2PortHandle port = 0;
    Require(sceAudioOut2LoPortCreate(invalidCtx, &portParam, &port) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortCreate(nonExistentCtx, &portParam, &port) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortCreate(ctx, nullptr, &port) == ErrorInvalidArgument);
    Require(sceAudioOut2LoPortCreate(ctx, &portParam, nullptr) == ErrorInvalidArgument);

    Require(sceAudioOut2LoPortCreateEx(invalidCtx, &portParam, &port) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortCreateEx(ctx, nullptr, &port) == ErrorInvalidArgument);
    Require(sceAudioOut2LoPortCreateEx(ctx, &portParam, nullptr) == ErrorInvalidArgument);

    portParam.data_format = 0x80u | (2u << 8);
    Require(sceAudioOut2LoPortCreate(ctx, &portParam, &port) == ErrorInvalidParam);

    portParam.data_format = 6u << 8;
    Require(sceAudioOut2LoPortCreate(ctx, &portParam, &port) == ErrorUnsupportedFormat);

    portParam.data_format = (2u << 8) | 2;
    Require(sceAudioOut2LoPortCreate(ctx, &portParam, &port) == ErrorUnsupportedFormat);

    portParam.port_type = 1;
    portParam.data_format = 0x10000 | (2u << 8);
    bool threwUnimplementedFormat = false;
    try {
        sceAudioOut2LoPortCreate(ctx, &portParam, &port);
    } catch (const std::runtime_error&) {
        threwUnimplementedFormat = true;
    }
    Require(threwUnimplementedFormat);
    portParam.port_type = 0;

    portParam.data_format = 2u << 8;
    Require(sceAudioOut2LoPortCreate(ctx, &portParam, &port) == 0);
    Require(port != 0);

    constexpr AudioOut2PortHandle invalidPort = 0;
    constexpr AudioOut2PortHandle nonExistentPort = 0x4321;
    AudioOut2Attribute attr{};

    Require(sceAudioOut2LoPortSetAttributes(invalidPort, &attr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortSetAttributes(nonExistentPort, &attr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortSetAttributes(port, nullptr, 1) == ErrorInvalidArgument);
    Require(sceAudioOut2LoPortSetAttributes(port, nullptr, 0) == 0);
    Require(sceAudioOut2LoPortSetAttributes(port, &attr, 0) == 0);

    Require(sceAudioOut2LoPortSetAttributesEx(invalidPort, &attr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortSetAttributesEx(nonExistentPort, &attr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortSetAttributesEx(port, nullptr, 1) == ErrorInvalidArgument);
    Require(sceAudioOut2LoPortSetAttributesEx(port, nullptr, 0) == 0);
    Require(sceAudioOut2LoPortSetAttributesEx(port, &attr, 0) == 0);

    Require(sceAudioOut2LoPortGetAttributes(invalidPort, &attr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortGetAttributes(nonExistentPort, &attr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortGetAttributes(port, nullptr, 1) == ErrorInvalidParam);
    Require(sceAudioOut2LoPortGetAttributes(port, nullptr, 0) == 0);

    attr.attribute_id = 0x77;
    Require(sceAudioOut2LoPortGetAttributes(port, &attr, 1) == ErrorInvalidParam);

    for (const std::uint32_t unsupportedId : {9u, 0xAu, 0x21u}) {
        bool threwAttr = false;
        try {
            attr.attribute_id = unsupportedId;
            sceAudioOut2LoPortGetAttributes(port, &attr, 1);
        } catch (const std::runtime_error&) {
            threwAttr = true;
        }
        Require(threwAttr);
    }

    float volumes[2] = {0.0f, 0.0f};
    attr.attribute_id = 1;
    attr.value = volumes;
    attr.value_size = sizeof(volumes);
    Require(sceAudioOut2LoPortGetAttributes(port, &attr, 1) == 0);

    AudioOut2PortState portState{};
    Require(sceAudioOut2LoPortGetState(invalidPort, &portState) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortGetState(nonExistentPort, &portState) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortGetState(port, nullptr) == ErrorInvalidArgument);

    std::memset(&portState, 0x55, sizeof(portState));
    Require(sceAudioOut2LoPortGetState(port, &portState) == 0);
    Require(portState.output == 1);
    Require(portState.num_channels == 2);
    Require(portState.volume == 127);
    Require(portState.reroute_counter == 0);
    Require(portState.flags == 0);

    bool threwStateEx = false;
    try {
        sceAudioOut2LoPortGetStateEx();
    } catch (const std::runtime_error&) {
        threwStateEx = true;
    }
    Require(threwStateEx);

    Require(sceAudioOut2LoPortDestroy(port) == 0);
    Require(sceAudioOut2LoPortDestroy(port) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortDestroy(invalidPort) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortDestroy(nonExistentPort) == ErrorInvalidHandle);

    Require(sceAudioOut2LoPortGetState(port, &portState) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortSetAttributes(port, &attr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortSetAttributesEx(port, &attr, 1) == ErrorInvalidHandle);
    Require(sceAudioOut2LoPortGetAttributes(port, &attr, 1) == ErrorInvalidHandle);
}

} // namespace

int main() {
    TestSet3DLatencyBounds();
    TestLowLatencyLifecycle();
    TestLowLatencyContextQueryMemoryBounds();

    AudioOut2ContextHandle ctx = 0;
    std::vector<std::byte> buffer;
    TestLowLatencyContextCreateBounds(ctx, buffer);
    Require(ctx != 0);

    TestLowLatencyContextOperationsBounds(ctx);
    TestLowLatencyPortLifecycleAndAttributeBounds(ctx);

    Require(sceAudioOut2LoContextDestroy(ctx) == 0);
    Require(sceAudioOut2LoContextDestroy(ctx) == ErrorInvalidHandle);
    Require(sceAudioOut2LoContextAdvance(ctx) == ErrorInvalidHandle);
    Require(sceAudioOut2LoContextPush(ctx, 0) == ErrorInvalidHandle);

    std::uint32_t qLevel = 0;
    std::uint32_t qAvail = 0;
    Require(sceAudioOut2LoContextGetQueueLevel(ctx, &qLevel, &qAvail) == ErrorInvalidHandle);

    Require(sceAudioOut2LoTerminate() == 0);
    LibcRunShutdown_nid_postfix();
    return 0;
}
