#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;

alignas(256) constexpr std::array<std::uint32_t, 19> UserCaller{
    0x7e020280u, 0xbe940383u, 0xbe8e0300u, 0xbe8f0301u, 0xbe8e210eu, 0xbe8e0302u, 0xbe8f0303u, 0xbe8e210eu,
    0x80948114u, 0xbf078014u, 0xbf85fff7u, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u, 0xbf9f0000u,
    0x30306c73u, 0x00000040u, 0x00000008u};
alignas(256) constexpr std::array<std::uint32_t, 20> TableCaller{
    0x7e020280u, 0xf4080400u, 0xfa000000u, 0x8f148402u, 0xbf8cc07fu, 0xf4280208u, 0x28000000u, 0xbf8cc07fu,
    0xbe8e0308u, 0xbe8f0309u, 0xbf068103u, 0xbe8e210eu, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u,
    0xbf9f0000u, 0x30306c73u, 0x00000040u, 0x00000008u};
alignas(256) constexpr std::array<std::uint32_t, 3> Sixteen{0x85159091u, 0x7e020215u, 0xbe80200eu};
alignas(256) constexpr std::array<std::uint32_t, 3> ThirtyTwo{0x8515a0a1u, 0x7e020215u, 0xbe80200eu};
alignas(256) std::array<std::uint32_t, 256> Output{};

void Check(AgcDriver::VulkanDevice& device, const std::string& kind) {
    alignas(256) std::array<std::uint32_t, 2> callee{0x4a020281u, 0xbe80200eu};
    const auto first = reinterpret_cast<std::uintptr_t>(Sixteen.data());
    const auto second = reinterpret_cast<std::uintptr_t>(ThirtyTwo.data());
    alignas(16) std::array<std::uint32_t, 8> table{static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(first >> 32u), 0, 0, static_cast<std::uint32_t>(second), static_cast<std::uint32_t>(second >> 32u), 0, 0};
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(table.data());
    alignas(16) std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u) | (16u << 16u), kind == "descriptor" ? 1u : 2u, 0x31016facu};
    const auto pointer = reinterpret_cast<std::uintptr_t>(kind == "callee" ? callee.data() : descriptor.data());
    const auto output = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 8> users{static_cast<std::uint32_t>(pointer), static_cast<std::uint32_t>(pointer >> 32u), kind == "callee" ? static_cast<std::uint32_t>(pointer) : 0u, kind == "callee" ? static_cast<std::uint32_t>(pointer >> 32u) : 0u,
        static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu), sizeof(Output), 0x31016facu};
    const std::span<const std::uint32_t> code = kind == "callee" ? std::span<const std::uint32_t>(UserCaller) : std::span<const std::uint32_t>(TableCaller);
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{reinterpret_cast<std::uintptr_t>(code.data()), 0, 0, {code.begin(), code.end()}, {}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {false, false, false}, false, 1, {}};
    const ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Compute, snapshot.codeAddress, snapshot.code, 0, {}}, {32, 0, users, compute, {}, {}, {}}, device.ComputeTarget(32), {0, 0, 0, 128}};
    const auto address = kind == "callee" ? pointer : kind == "descriptor" ? pointer + 8 : tableAddress;
    const std::vector<std::uint32_t> replacement = kind == "callee" ? std::vector<std::uint32_t>{0x4a020285u} : kind == "descriptor" ? std::vector<std::uint32_t>{2u} : std::vector<std::uint32_t>{static_cast<std::uint32_t>(second), static_cast<std::uint32_t>(second >> 32u)};
    const auto drains = DrawPipeline::DrainRequestsByOpcode(0x100);
    std::atomic<bool> returned{false}, started{false}, timedOut{false};
    struct Restore {
        std::atomic<bool>& returned;
        bool active = DrawPipeline::Active();
        ~Restore() {
            returned.store(true, std::memory_order_release);
            DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
            DrawPipeline::Active() = active;
        }
    } restore{returned};
    DrawPipeline::Queue0().Enqueue([&] {
        started.store(true, std::memory_order_release);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!returned.load(std::memory_order_acquire) && DrawPipeline::DrainRequestsByOpcode(0x100) == drains) {
            if (std::chrono::steady_clock::now() >= deadline) {
                timedOut.store(true, std::memory_order_release);
                break;
            }
            std::this_thread::yield();
        }
        AgcDriver::GuestMemory::Write(address, std::as_bytes(std::span(replacement)), alignof(std::uint32_t));
    }, {{address, address + replacement.size() * sizeof(std::uint32_t)}});
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!started.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("queued shader call producer did not start");
        std::this_thread::yield();
    }
    DrawPipeline::Active() = true;
    const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
    const auto linked = invocation.Request().shader.code;
    const std::vector<std::uint32_t> captured(linked.begin(), linked.end());
    returned.store(true, std::memory_order_release);
    DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
    const auto expected = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
    if (timedOut.load(std::memory_order_acquire)) throw std::runtime_error("queued shader call producer timed out");
    if (!std::ranges::equal(captured, expected.Request().shader.code)) throw std::runtime_error("shader invocation ignored queued " + kind + " write");
}

}

int main(int argc, char** argv) {
    try {
        if (DrawPipeline::Depth() == 0) return VulkanTestSkipped;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto* kind : {"callee", "descriptor", "table"}) {
            if (argc == 1 || kind == std::string(argv[1])) Check(*device, kind);
        }
        DrawPipeline::Queue0().Stop();
        std::cout << "Shader calls observed queued code, descriptor and table writes\n";
        return 0;
    } catch (const std::exception& error) {
        DrawPipeline::Queue0().Stop();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
