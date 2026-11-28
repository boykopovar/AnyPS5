#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CommandEncoder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "EncoderReplayGate.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;

struct InjectedFailure {};

bool MeshArgumentsOutliveAThrowAfterTheirKernel(const Context& context, Recorder& recorder, const Buffer& record) {
    constexpr std::uint64_t GuestArguments = 0x7000'0000;
    State state{};
    state.stages.mesh = ShaderRecompiler::MeshConfiguration{4u, 32u, 96u, 96u, 32u, 128u, 2048u, 0u, 4u};
    AgcDriver::Pm4::DrawParameters draw{};
    draw.indexCount = 96;
    draw.instanceCount = 1;
    AgcDriver::Pm4::DrawParameters::IndirectDraw args{};
    args.arguments = GuestArguments;
    HostImport argumentImport{};
    argumentImport.base = GuestArguments;
    argumentImport.bytes = 4096;
    argumentImport.address = record.DeviceAddress();
    std::shared_ptr<Buffer> arguments;
    std::weak_ptr<Buffer> queuedFor;
    bool ownedWhenQueued = false;
    bool threw = false;
    bool observed = false;
    std::atomic<bool> aliveAtReplay{false};
    const auto commands = recorder.Commands();
    {
        EncoderReplayGate gate([&] { aliveAtReplay.store(!queuedFor.expired(), std::memory_order_release); });
        try {
            Recorder::EncodingWindow window(recorder);
            Require(window.Encoded(), "the mesh argument window was not encoded");
            const auto unwindGuard = recorder.UnwindGuard();
            RecordMeshArgumentsForTesting(context, commands, recorder, state, draw, args, argumentImport, [&](std::uint32_t count) {
                if (count != 2) return;
                ownedWhenQueued = arguments != nullptr;
                queuedFor = arguments;
                throw InjectedFailure{};
            }, arguments);
        } catch (const InjectedFailure&) {
            threw = true;
        }
        observed = gate.Observed();
    }
    recorder.Sync();
    if (!threw) return false;
    Require(ownedWhenQueued, "the mesh argument buffer was not owned by the caller when its kernel was queued, so a throw before the helper returned released it under the queued kernel");
    Require(observed && aliveAtReplay.load(std::memory_order_acquire), "the mesh argument buffer was released before the encoder replayed the kernel that writes it");
    return true;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        if (!context.meshShader) {
            std::printf("skipped, the device has no mesh shader support\n");
            return VulkanTestSkipped;
        }
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr, "the device has no active recorder");
        if (!recorder->Encodes()) {
            std::printf("skipped, the command encoder is off\n");
            return VulkanTestSkipped;
        }
        Require(context.functions != nullptr && context.functions->cmdPipelineBarrier != nullptr, "the encoded device table is unavailable");
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        constexpr auto usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        Buffer record(context, 4096, usage);
        Buffer arguments(context, 4096, usage);
        const auto commands = recorder->Commands();
        const auto before = recorder->EncoderCounts();
        const std::array<std::uint32_t, 8> rules{};
        bool recorded = false;
        {
            Recorder::EncodingWindow window(*recorder);
            const VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT};
            context.functions->cmdPipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
            recorded = recorder->RecordMeshArguments(commands, record.DeviceAddress(), arguments.DeviceAddress(), rules);
        }
        const auto after = recorder->EncoderCounts();
        if (!recorded) {
            std::printf("skipped, the mesh argument kernel is unavailable\n");
            return VulkanTestSkipped;
        }
        const auto queued = after.commands - before.commands;
        Require(queued == 6, "the mesh argument kernel bypassed the encoder window: " + std::to_string(queued) + " commands queued, expected the leading barrier plus the kernel's barrier, bind, push constants, dispatch and barrier");
        recorder->Sync();
        Require(MeshArgumentsOutliveAThrowAfterTheirKernel(context, *recorder, record), "the mesh argument kernel was not queued on the GPU indirect path");
        std::cout << "encoded mesh arguments test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
