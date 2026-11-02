#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Untouched = 0xcafebabeu;

alignas(256) constexpr std::array<std::uint32_t, 19> Caller{
    0x7e020280u, 0xbe940383u, 0xbe8e0300u, 0xbe8f0301u, 0xbe8e210eu, 0xbe8e0302u, 0xbe8f0303u, 0xbe8e210eu,
    0x80948114u, 0xbf078014u, 0xbf85fff7u, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u, 0xbf9f0000u,
    0x30306c73u, 0x00000040u, 0x00000008u};
alignas(256) std::array<std::uint32_t, 2> First{};
alignas(256) std::array<std::uint32_t, 2> Second{};
alignas(256) std::array<std::uint32_t, Threads> Output{};

std::array<std::uint32_t, 2> AddInline(std::uint32_t value) {
    return {0x4a020280u + value, 0xbe80200eu};
}

std::array<std::uint32_t, 8> UserData(const void* first, const void* second) {
    const auto low = reinterpret_cast<std::uintptr_t>(first);
    const auto high = reinterpret_cast<std::uintptr_t>(second);
    const auto output = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(low), static_cast<std::uint32_t>(low >> 32u), static_cast<std::uint32_t>(high), static_cast<std::uint32_t>(high >> 32u),
        static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu};
}

struct Fixture {
    AgcDriver::VulkanDevice& device;
    AgcDriver::DriverDetail::ShaderSnapshot snapshot;
    std::array<std::uint32_t, 8> users{};
    ShaderRecompiler::RecompileRequest request;

    explicit Fixture(AgcDriver::VulkanDevice& device) : device(device), snapshot{reinterpret_cast<std::uintptr_t>(Caller.data()), 0, 0, {Caller.begin(), Caller.end()}, {}} {
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
        request = {{ShaderRecompiler::ShaderStage::Compute, snapshot.codeAddress, snapshot.code, 0, {}}, {Lanes, 0, users, compute, std::nullopt, std::nullopt, {}}, device.ComputeTarget(Lanes), {0, 0, 0, 128}};
        request.useCache = false;
    }

    std::vector<AgcDriver::DriverDetail::PreparedShaderState::UserDataLink> Links() const {
        std::lock_guard lock(snapshot.prepared->mutex);
        return snapshot.prepared->userDataLinks;
    }

    void Check(const void* first, const void* second, std::uint32_t expected, const std::string& what) {
        users = UserData(first, second);
        request.context.userData = users;
        const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
        ShaderRecompiler::SrtRuntime runtime{};
        runtime.userData = users;
        const auto capture = invocation.Capture(runtime);
        const auto result = invocation.Materialize(*capture);
        Output.fill(Untouched);
        device.Dispatch(*result, 1, 1, 1);
        device.WaitIdle();
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            Require(Output[lane] == expected, what + ": lane " + std::to_string(lane) + " is " + std::to_string(Output[lane]) + ", expected " + std::to_string(expected));
        }
    }
};

void Run(AgcDriver::VulkanDevice& device) {
    Fixture fixture(device);
    First = AddInline(5u);
    Second = AddInline(64u);
    fixture.Check(First.data(), Second.data(), 3u * (5u + 64u), "first link");
    auto links = fixture.Links();
    Require(links.size() == 1u && links.front().linked != nullptr && links.front().callees.size() == 2u, "the first invocation did not keep exactly one user-data link with both callees");
    const auto firstLink = links.front().linked;
    Require(!firstLink->code.empty(), "the kept user-data link has no linked code");

    fixture.Check(First.data(), Second.data(), 3u * (5u + 64u), "same callees again");
    links = fixture.Links();
    Require(links.size() == 1u && links.front().linked == firstLink, "an invocation with unchanged callees linked the program again instead of reusing its link");

    First = AddInline(1u);
    fixture.Check(First.data(), Second.data(), 3u * (1u + 64u), "first callee rewritten in place");
    links = fixture.Links();
    Require(links.size() == 2u && links.back().linked != firstLink && links.back().callees == links.front().callees, "a callee rewritten at the same address did not get a new link");
    const auto rewrittenLink = links.back().linked;
    Require(!std::ranges::equal(rewrittenLink->code, firstLink->code), "the link of a rewritten callee kept the old callee's code");

    First = AddInline(5u);
    fixture.Check(First.data(), Second.data(), 3u * (5u + 64u), "first callee restored");
    links = fixture.Links();
    Require(links.size() == 2u, "restoring a callee's earlier code linked the program again instead of reusing the earlier link");

    fixture.Check(Second.data(), Second.data(), 3u * (64u + 64u), "both pointers at the second callee");
    links = fixture.Links();
    Require(links.size() == 3u && links.back().callees.front() == links.back().callees.back(), "callees moved to another address did not get a new link");

    for (std::uint32_t value = 10; value < 20; ++value) {
        First = AddInline(value);
        fixture.Check(First.data(), Second.data(), 3u * (value + 64u), "first callee rewritten to add " + std::to_string(value));
    }
    links = fixture.Links();
    Require(links.size() == 8u && links.back().code.front() == std::vector<std::uint32_t>(First.begin(), First.end()), "the user-data links of one program are not limited to the eight most recent");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        std::puts("user-data link reuse tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
