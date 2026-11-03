#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::BindlessHeap;
using AgcDriver::Graphics::Buffer;
using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;

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

void RunDispatchArguments(const AgcDriver::VulkanDevice& device) {
    const auto context = device.graphicsContext();
    const bool heapMode = AgcDriver::Graphics::DescriptorHeapEnabled() && context.descriptorHeap && BindlessHeap::For(context).DescriptorHeapMode();
    constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    auto threads = std::make_shared<Buffer>(context, 16, usage);
    auto oversized = std::make_shared<Buffer>(context, 16, usage);
    auto grouped = std::make_shared<Buffer>(context, 32, usage);
    auto refused = std::make_shared<Buffer>(context, 32, usage);
    const std::uint32_t threadCounts[4] = {3u, 5u, 7u, 0u};
    const auto* limit = context.limits.maxComputeWorkGroupCount;
    const auto axis = static_cast<std::size_t>(std::min_element(limit, limit + 3) - limit);
    const bool limited = limit[axis] < std::numeric_limits<std::uint32_t>::max();
    std::uint32_t tooMany[4] = {1u, 1u, 1u, 0u};
    tooMany[axis] = limited ? limit[axis] + 1u : limit[axis];
    std::memcpy(threads->Bytes().data(), threadCounts, sizeof(threadCounts));
    std::memcpy(oversized->Bytes().data(), tooMany, sizeof(tooMany));
    std::memset(grouped->Bytes().data(), 0xff, 32);
    std::memset(refused->Bytes().data(), 0xff, 32);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "push-only heap passes: no active recorder");
    if (heapMode) BindlessHeap::For(context).BindDescriptorHeaps(recorder->Commands());
    const auto binds = AgcDriver::Graphics::DescriptorHeapBinds();
    const std::array<std::uint32_t, 3> group{2u, 2u, 2u};
    Require(recorder->RecordDispatchArguments(recorder->Commands(), threads->DeviceAddress(), grouped->DeviceAddress(), &group), "push-only heap passes: the dispatch argument kernel is unavailable");
    Require(recorder->RecordDispatchArguments(recorder->Commands(), oversized->DeviceAddress(), refused->DeviceAddress()), "push-only heap passes: the dispatch argument kernel is unavailable");
    if (heapMode) {
        BindlessHeap::For(context).BindDescriptorHeaps(recorder->Commands());
        Require(AgcDriver::Graphics::DescriptorHeapBinds() == binds, "push-only heap passes: the dispatch argument check put its command buffer back in classic descriptor mode, so the heaps were bound again");
    }
    for (const auto& kept : {threads, oversized, grouped, refused}) recorder->Keep(kept);
    recorder->Submit();
    recorder->Sync();
    std::uint32_t words[6]{};
    std::memcpy(words, grouped->Bytes().data(), sizeof(words));
    const std::uint32_t expected[6] = {2u, 3u, 4u, 3u, 5u, 7u};
    for (std::size_t i = 0; i < 6; ++i) Require(words[i] == expected[i], "push-only heap passes: dispatch argument word " + std::to_string(i) + " is " + std::to_string(words[i]) + ", expected " + std::to_string(expected[i]));
    std::memcpy(words, refused->Bytes().data(), 3 * sizeof(std::uint32_t));
    if (limited) Require(words[0] == 0u && words[1] == 0u && words[2] == 0u, "push-only heap passes: a group count past the device limit was dispatched");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RunChangedCopy(*device);
        RunDispatchArguments(*device);
        device->WaitIdle();
        std::puts("push-only heap pass tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
