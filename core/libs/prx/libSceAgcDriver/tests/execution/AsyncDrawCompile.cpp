#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "RuntimeAbi.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 7> Code{0xd7460000u, 0x04010c04u, 0x7e0202ffu, 0x12345678u, 0xe0702000u, 0x80000100u, 0xbf810000u};
constexpr std::uint32_t StoredFormat = 0x00014204u;
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t Stored = 0x12345678u;
alignas(256) std::array<std::uint32_t, 64> Output{};

void Dispatch() {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> data{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(Output.size()), StoredFormat};
    const auto program = reinterpret_cast<std::uintptr_t>(Code.data());
    std::vector<std::uint32_t> words{
        0xc0037600u, 0x207, 64, 1, 1,
        0xc0027600u, 0x20c, static_cast<std::uint32_t>(program >> 8u), static_cast<std::uint32_t>(program >> 40u),
        0xc0017600u, 0x213, (4u << 1u) | (1u << 7u),
        0xc0047600u, 0x240, data[0], data[1], data[2], data[3],
        0xc0031500u, 1, 1, 1, 0x41u,
    };
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(address, sizeof(Output));
}

bool Done() {
    for (const auto word : Output) {
        if (word != Stored) return false;
    }
    return true;
}

}

std::string Ms(std::chrono::steady_clock::duration duration) {
    return std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0) + " ms";
}

void CheckSlidingWaitBudget() {
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;
    using Budget = AgcDriver::DriverDetail::SlidingWaitBudget;
    const auto start = Clock::time_point{} + 100s;
    {
        Budget budget(2000ms, 10s, 4000ms);
        const auto first = budget.Take(start);
        Require(first.allowed == 2000ms, "an unused compute pipeline wait budget did not allow a full wait (" + Ms(first.allowed) + ")");
        budget.Finish(first, start + 2s);
        const auto second = budget.Take(start + 2s);
        Require(second.allowed == 2000ms, "a compute pipeline wait budget with 2 of 4 s spent did not allow a full wait (" + Ms(second.allowed) + ")");
        budget.Finish(second, start + 4s);
        Require(budget.Take(start + 4s).allowed == Clock::duration{}, "a compute pipeline wait budget allowed more than its total in 10 s");
        const auto overlap = budget.Take(start + 11s);
        Require(overlap.allowed == 1000ms, "a compute pipeline wait budget did not count the waits that overlap the last 10 s (" + Ms(overlap.allowed) + " allowed)");
        budget.Finish(overlap, start + 11s);
        const auto later = budget.Take(start + 14s);
        Require(later.allowed == 2000ms, "a compute pipeline wait budget kept waits older than 10 s or counted an empty wait (" + Ms(later.allowed) + ")");
    }
    {
        Budget budget(500ms, 10s, 1000ms);
        const auto first = budget.Take(start);
        budget.Finish(first, start + 500ms);
        const auto second = budget.Take(start + 500ms);
        const auto third = budget.Take(start + 999ms);
        Require(second.allowed == 500ms, "a compute pipeline wait budget with 500 of 1000 ms spent did not allow the rest (" + Ms(second.allowed) + ")");
        Require(third.allowed == Clock::duration{}, "two compute pipeline waits outstanding at once were both given the same remaining budget (" + Ms(third.allowed) + " for the second)");
        budget.Finish(second, start + 600ms);
        const auto fourth = budget.Take(start + 600ms);
        Require(fourth.allowed == 400ms, "a wait that ended early did not return the rest of its reservation to the compute pipeline wait budget (" + Ms(fourth.allowed) + ")");
        budget.Finish(third, start + 999ms);
        budget.Finish(fourth, start + 600ms);
    }
    {
        Budget budget(600ms, 10s, 1000ms);
        const auto longer = budget.Take(start);
        const auto shorter = budget.Take(start + 1ms);
        Require(longer.allowed == 600ms && shorter.allowed == 400ms, "two outstanding compute pipeline waits did not split the budget (" + Ms(longer.allowed) + ", " + Ms(shorter.allowed) + ")");
        budget.Finish(longer, start + 600ms);
        budget.Finish(shorter, start + 2ms);
        const auto after = budget.Take(start + 10s + 50ms);
        Require(after.allowed == 450ms, "a compute pipeline wait that finished first but ended out of the window was not dropped from the budget (" + Ms(after.allowed) + " allowed, 450 ms expected)");
    }
}

void CheckFailedBuildAhead() {
    using Ahead = AgcDriver::VulkanDevice::PipelineAhead;
    auto device = OpenVulkanTestDevice();
    if (!device) return;
    ShaderRecompiler::RecompileResult failing;
    failing.spirv = std::vector<std::uint32_t>{0x07230203u, 0x00010300u, 0u, 16u, 0u};
    failing.runtimeAbiVersion = ShaderRecompiler::RuntimeAbi::Version + 1u;
    const auto first = device->ComputePipelineAhead(failing);
    if (first == Ahead::Unavailable) return;
    Require(first == Ahead::Start, "a compute pipeline that is not built yet was not handed out for a build ahead");
    Require(device->ComputePipelineAhead(failing) == Ahead::Building, "a compute pipeline queued for a build ahead was not reported as building");
    Require(!device->AwaitComputePipelineAhead(failing, std::chrono::steady_clock::now() + std::chrono::milliseconds(20)), "a dispatch waiting for a compute pipeline still queued for a build ahead went ahead before the build");
    bool threw = false;
    try {
        device->PrebuildComputePipeline(failing);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "a build ahead with an incompatible runtime ABI did not fail");
    Require(device->ComputePipelineAhead(failing) == Ahead::Unavailable, "a build ahead that failed left its compute pipeline queued, so every dispatch of it would be skipped");
    Require(device->AwaitComputePipelineAhead(failing, std::chrono::steady_clock::now()), "a dispatch waiting for a build ahead that failed was skipped instead of building the pipeline itself");
    ShaderRecompiler::RecompileResult abandoned;
    abandoned.spirv = std::vector<std::uint32_t>{0x07230203u, 0x00010300u, 0u, 17u, 0u};
    Require(device->ComputePipelineAhead(abandoned) == Ahead::Start, "a second compute pipeline was not handed out for a build ahead");
    device->AbandonComputePipelineAhead(abandoned);
    Require(device->ComputePipelineAhead(abandoned) == Ahead::Unavailable, "an abandoned build ahead left its compute pipeline queued");
}

int main() {
    try {
        CheckSlidingWaitBudget();
        CheckFailedBuildAhead();
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Output.fill(Untouched);
        const bool async = AgcDriver::DriverDetail::AsyncDrawCompile();
        const auto before = AgcDriver::VulkanDevice::ComputePipelineCounts();
        std::size_t attempts = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        do {
            Dispatch();
            ++attempts;
            if (Done()) break;
            for (const auto word : Output) Require(word == Untouched || word == Stored, "a skipped dispatch stored a partial result");
            Require(async, "a dispatch without APS5_ASYNC_DRAW_COMPILE was skipped");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (std::chrono::steady_clock::now() < deadline);
        Require(Done(), "the dispatch never stored after its shader was prepared and specialized in the background");
        Require(async || attempts == 1, "a dispatch without APS5_ASYNC_DRAW_COMPILE needed more than one attempt");
        const bool ahead = async && AgcDriver::DriverDetail::AsyncComputePipelineWords() == 0;
        const bool waits = async && AgcDriver::DriverDetail::AsyncDispatchWait().count() > 0 && (!ahead || AgcDriver::DriverDetail::AsyncComputeWait().count() > 0);
        Require(!waits || attempts == 1, "a dispatch whose shader was prepared and specialized in the background within APS5_ASYNC_DISPATCH_WAIT_MS, and whose compute pipeline was built ahead within APS5_ASYNC_COMPUTE_WAIT_MS, was skipped (" + std::to_string(attempts) + " attempts)");
        const auto after = AgcDriver::VulkanDevice::ComputePipelineCounts();
        Require(after.created > before.created, "the dispatch stored without creating a compute pipeline");
        if (ahead) Require(after.created - before.created == after.ahead - before.ahead, "a dispatch with APS5_ASYNC_DRAW_COMPILE and APS5_ASYNC_COMPUTE_WORDS=0 created its compute pipeline itself (" + std::to_string(after.created - before.created) + " created, " + std::to_string(after.ahead - before.ahead) + " on the library prebuild pool)");
        else Require(after.ahead == before.ahead, "a dispatch whose compute pipeline is below APS5_ASYNC_COMPUTE_WORDS, or without APS5_ASYNC_DRAW_COMPILE, built its pipeline on the library prebuild pool");
        AgcDriverShutdown_nid_postfix();
        std::printf("async draw compile tests passed (%s, %zu dispatches)\n", async ? "enabled" : "disabled", attempts);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
