#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "VulkanTestDevice.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::BindlessHeap;
using AgcDriver::Graphics::Buffer;
using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;

// The changed copy reads only push constants. It copies the same in both modes; in descriptor heap
// mode it must also leave its command buffer in heap mode (parameters through vkCmdPushDataEXT on a
// heap pipeline): a classic vkCmdPushConstants would make the next heap draw bind the heaps again.
void RunChangedCopy(const AgcDriver::VulkanDevice& device) {
    const auto context = device.graphicsContext();
    const bool heapMode = AgcDriver::Graphics::DescriptorHeapEnabled() && context.descriptorHeap && BindlessHeap::For(context).DescriptorHeapMode();
    constexpr std::uint32_t words = 300;
    constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    auto result = std::make_shared<Buffer>(context, words * 4u, usage);
    auto snapshot = std::make_shared<Buffer>(context, words * 4u, usage);
    auto target = std::make_shared<Buffer>(context, words * 4u, usage);
    std::vector<std::uint32_t> shader(words);
    for (std::uint32_t i = 0; i < words; ++i) shader[i] = i % 3 == 0 ? 0x9e3779b9u * (i + 1u) : 0u;
    std::memset(snapshot->Bytes().data(), 0, words * 4u);
    std::memset(target->Bytes().data(), 0, words * 4u);
    std::memcpy(result->Bytes().data(), shader.data(), words * 4u);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "push-only heap passes: no active recorder");
    if (heapMode) BindlessHeap::For(context).BindDescriptorHeaps(recorder->Commands());
    const auto binds = AgcDriver::Graphics::DescriptorHeapBinds();
    Require(recorder->RecordChangedCopy(recorder->Commands(), result->DeviceAddress(), snapshot->DeviceAddress(), target->DeviceAddress(), words), "push-only heap passes: the changed copy kernel is unavailable");
    if (heapMode) {
        BindlessHeap::For(context).BindDescriptorHeaps(recorder->Commands());
        Require(AgcDriver::Graphics::DescriptorHeapBinds() == binds, "push-only heap passes: the changed copy put its command buffer back in classic descriptor mode, so the heaps were bound again");
    }
    recorder->Keep(result);
    recorder->Keep(snapshot);
    recorder->Keep(target);
    recorder->Submit();
    recorder->Sync();
    std::vector<std::uint32_t> stored(words);
    std::memcpy(stored.data(), target->Bytes().data(), words * 4u);
    for (std::uint32_t i = 0; i < words; ++i) {
        Require(stored[i] == shader[i], "push-only heap passes: changed copy word " + std::to_string(i) + " is " + std::to_string(stored[i]) + ", the shader wrote " + std::to_string(shader[i]));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RunChangedCopy(*device);
        device->WaitIdle();
        std::puts("push-only heap pass tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
