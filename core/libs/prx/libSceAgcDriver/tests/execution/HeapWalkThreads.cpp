#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::size_t Variants = 1024;
constexpr std::size_t Rounds = 3;
constexpr std::size_t Held = 48;
alignas(256) std::array<std::uint32_t, Lanes * 4u> Input{};
alignas(256) std::array<std::uint32_t, Lanes * 4u> Output{};
alignas(256) constexpr std::array<std::uint32_t, 16> Code{0x34020082u, 0xe0001000u, 0x80000201u, 0xe0301000u, 0x80000301u, 0xbf8c0000u,
                                                          0xe0101000u, 0x80000201u, 0xe0701000u, 0x80000301u,
                                                          0xe0701000u, 0x80010201u, 0xe0701080u, 0x80010301u, 0xbf800000u, 0xbf810000u};

std::array<std::uint32_t, 4> Descriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    const auto word3 = (((0x01016facu & ~(0x7fu << 12u)) | (Format32UInt << 12u) | (3u << 28u)) & ~(3u << 30u));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, word3};
}

struct Variant {
    std::vector<std::uint32_t> userData;
    ShaderRecompiler::RecompileRequest request;
    std::optional<ShaderRecompiler::PreparedShaderInvocation> invocation;
};

std::vector<std::unique_ptr<Variant>> Prepare(AgcDriver::VulkanDevice& device) {
    static const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    std::vector<std::unique_ptr<Variant>> variants;
    for (std::size_t index = 0; index < Variants; ++index) {
        auto variant = std::make_unique<Variant>();
        const auto inputBytes = static_cast<std::uint32_t>(sizeof(Input) - 4u * (index % 64u));
        const auto outputBytes = static_cast<std::uint32_t>(sizeof(Output) - 4u * (index / 64u));
        const auto input = Descriptor(Input.data(), inputBytes);
        const auto output = Descriptor(Output.data(), outputBytes);
        variant->userData.insert(variant->userData.end(), input.begin(), input.end());
        variant->userData.insert(variant->userData.end(), output.begin(), output.end());
        variant->request = ShaderRecompiler::RecompileRequest{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
            {Lanes, 0, variant->userData, compute, std::nullopt, std::nullopt, memory},
            device.Target(),
            {0, 0, 0, 128}};
        const auto handle = ShaderRecompiler::PrepareShader(variant->request);
        variant->invocation = ShaderRecompiler::PreparedShaderInvocation::TryCreate(variant->request, handle);
        Require(variant->invocation.has_value(), "the prepared shader refused its own request");
        variants.push_back(std::move(variant));
    }
    return variants;
}

struct Inbox {
    std::mutex mutex;
    std::deque<AgcDriver::DriverDetail::HeapStageMaterialized> stages;
};

std::size_t Touch(const AgcDriver::DriverDetail::HeapStageMaterialized& stage) {
    std::size_t sum = stage.result->spirv.size() + stage.words->userData.size() + stage.words->bindings.size();
    if (stage.reads.capture != nullptr) sum += stage.reads.capture->userDataUse.reads.size();
    if (stage.reads.values != nullptr) sum += stage.reads.values->size();
    return sum;
}

void Work(const std::vector<std::unique_ptr<Variant>>& variants, std::size_t first, Inbox& own, Inbox& peer, std::atomic<bool>& failed, std::string& error) {
    try {
        std::deque<AgcDriver::DriverDetail::HeapStageMaterialized> held;
        std::size_t touched = 0;
        for (std::size_t round = 0; round < Rounds; ++round) {
            for (std::size_t step = 0; step < variants.size(); ++step) {
                const auto& variant = *variants[(first + step * 7u) % variants.size()];
                auto stage = AgcDriver::DriverDetail::MaterializeHeapStage(*variant.invocation, variant.userData, variant.request.shader.codeAddress);
                Require(stage.result != nullptr && stage.words != nullptr, "a heap walk over valid buffer descriptors declined its stage");
                {
                    std::lock_guard lock(peer.mutex);
                    peer.stages.push_back(std::move(stage));
                }
                std::deque<AgcDriver::DriverDetail::HeapStageMaterialized> received;
                {
                    std::lock_guard lock(own.mutex);
                    received.swap(own.stages);
                }
                for (auto& item : received) {
                    touched += Touch(item);
                    held.push_back(std::move(item));
                }
                while (held.size() > Held) held.pop_front();
                if (failed.load(std::memory_order_relaxed)) return;
            }
        }
        Require(touched != 0, "a heap walk thread received no stage from the other thread");
    } catch (const std::exception& caught) {
        error = caught.what();
        failed.store(true, std::memory_order_relaxed);
    }
}

void TwoThreadsShareWalks(AgcDriver::VulkanDevice& device) {
    const auto variants = Prepare(device);
    Inbox first, second;
    std::atomic<bool> failed{false};
    std::string firstError, secondError;
    std::thread one(Work, std::cref(variants), std::size_t{0}, std::ref(first), std::ref(second), std::ref(failed), std::ref(firstError));
    std::thread two(Work, std::cref(variants), variants.size() / 2u, std::ref(second), std::ref(first), std::ref(failed), std::ref(secondError));
    one.join();
    two.join();
    Require(firstError.empty() && secondError.empty(), "a heap walk thread failed: " + firstError + secondError);
    std::puts("two heap walk threads exchanged their stages");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        TwoThreadsShareWalks(*device);
        std::puts("heap walk thread tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
