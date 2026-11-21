#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <mutex>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        using namespace AgcDriver::Graphics;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
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
        const std::array<std::uint32_t, 7> rules{};
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
        std::cout << "encoded mesh arguments test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
