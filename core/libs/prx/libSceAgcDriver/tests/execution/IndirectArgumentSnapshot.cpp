#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static std::shared_ptr<VulkanDevice> GetDevice() { return Driver::Get().device.Load(); }
    static void WaitQueue() {
        auto& driver = Driver::Get();
        std::unique_lock lock(driver.mutex);
        const auto target = driver.accepted;
        ++driver.idleWaiters;
        driver.changed.wait(lock, [&] { return driver.failure != nullptr || driver.stopping || driver.completed >= target; });
        --driver.idleWaiters;
        driver.rethrowFailure();
        driver.checkStopping();
    }
};

}

namespace {

using AgcDriver::Graphics::Require;

constexpr std::byte Kept{0x40};
constexpr std::uint32_t NoLocation = 0x280;

alignas(256) constexpr std::array<std::uint32_t, 20> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060208u, 0x7e0802f2u, 0x7e0a02ffu, 0x44800000u, 0xf80000d1u, 0x00000005u, 0xf80008cfu, 0x04030201u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 7> AddressWriterCode{0x7e000202u, 0x7e020203u, 0x7e040204u, 0x7e060280u, 0xdc7c8000u, 0x00000003u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
constexpr std::size_t GuestBytes = 1u << 20u;

struct Guest {
    alignas(256) std::array<std::byte, 1024> target{};
    alignas(256) std::array<std::uint32_t, 4> reused{3, 1, 0, 0};
    alignas(256) std::array<std::uint32_t, 4> reference{3, 1, 0, 0};
    alignas(256) std::array<std::uint32_t, 4> produced{};
};

Guest* Memory = nullptr;
std::array<std::byte, 1024> ReferenceBytes{};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

void Registered(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void Produce(std::vector<std::uint32_t>& commands, std::array<std::uint32_t, 4>& records) {
    const auto code = reinterpret_cast<std::uintptr_t>(AddressWriterCode.data());
    const auto record = reinterpret_cast<std::uintptr_t>(records.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0x20c, static_cast<std::uint32_t>(code >> 8u)}, {0x20d, static_cast<std::uint32_t>(code >> 40u) & 0xffu},
        {0x207, 1}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 5u << 1u},
        {0x240, static_cast<std::uint32_t>(record)}, {0x241, static_cast<std::uint32_t>(record >> 32u)}, {0x242, 3}, {0x243, 1}, {0x244, 0},
    };
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x15, {1, 1, 1, 0x8041});
}

std::vector<std::uint32_t> Draw(std::array<std::byte, 1024>& memory, const std::array<std::uint32_t, 4>& records) {
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const auto record = reinterpret_cast<std::uintptr_t>(records.data());
    const auto color = reinterpret_cast<std::uintptr_t>(memory.data());
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0x01200000}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    Append(commands, 0x79, {0x24a, 0});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x11, {1, static_cast<std::uint32_t>(record), static_cast<std::uint32_t>(record >> 32u)});
    Append(commands, 0x24, {0, NoLocation, NoLocation, 2});
    return commands;
}

std::size_t Covered(const std::array<std::byte, 1024>& memory) {
    return static_cast<std::size_t>(std::count_if(memory.begin(), memory.end(), [](std::byte value) { return value != Kept; }));
}

void Submit(std::vector<std::uint32_t>& commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
}

void ReferenceDraw() {
    Memory->target.fill(Kept);
    auto commands = Draw(Memory->target, Memory->reference);
    Submit(commands);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Memory->target.data()), Memory->target.size());
    Require(Covered(Memory->target) != 0, "the reference indirect draw, whose record draws 3 vertices, covered nothing");
    ReferenceBytes = Memory->target;
}

void ProducedRecordKeepsTheDraw() {
    Memory->target.fill(Kept);
    Memory->produced.fill(0);
    std::vector<std::uint32_t> commands;
    Produce(commands, Memory->produced);
    const auto draw = Draw(Memory->target, Memory->produced);
    commands.insert(commands.end(), draw.begin(), draw.end());
    Submit(commands);
    AgcDriverWaitIdle_nid_postfix();
    Require(Memory->produced[0] == 3 && Memory->produced[1] == 1, "the address-based store did not write the indirect draw record");
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Memory->target.data()), Memory->target.size());
    for (std::size_t index = 0; index < Memory->target.size(); ++index) {
        Require(Memory->target[index] == ReferenceBytes[index], "byte " + std::to_string(index) + " of the indirect draw whose record an address-based store ahead of it wrote is " + std::to_string(static_cast<unsigned>(Memory->target[index])) + ", the record it wrote draws " + std::to_string(static_cast<unsigned>(ReferenceBytes[index])));
    }
}

void ReusedRecordKeepsTheDraw() {
    Memory->target.fill(Kept);
    const auto device = AgcDriver::DriverDetail::DriverTestAccess::GetDevice();
    Require(device != nullptr, "the driver has no device after the reference draw");
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &type};
    VkSemaphore hold = VK_NULL_HANDLE;
    Require(context.Function<PFN_vkCreateSemaphore>("vkCreateSemaphore")(context.device, &info, nullptr, &hold) == VK_SUCCESS, "vkCreateSemaphore failed");
    auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr, "the driver has no active recorder");
    recorder->HoldNextSubmit(hold, 1);
    auto commands = Draw(Memory->target, Memory->reused);
    Submit(commands);
    AgcDriver::DriverDetail::DriverTestAccess::WaitQueue();
    Memory->reused.fill(0);
    VkSemaphoreSignalInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, nullptr, hold, 1};
    PFN_vkSignalSemaphore signalSemaphore = nullptr;
    try { signalSemaphore = context.Function<PFN_vkSignalSemaphore>("vkSignalSemaphoreKHR"); } catch (const std::exception&) { signalSemaphore = context.Function<PFN_vkSignalSemaphore>("vkSignalSemaphore"); }
    Require(signalSemaphore(context.device, &signal) == VK_SUCCESS, "vkSignalSemaphore failed");
    AgcDriverWaitIdle_nid_postfix();
    context.Function<PFN_vkDestroySemaphore>("vkDestroySemaphore")(context.device, hold, nullptr);
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Memory->target.data()), Memory->target.size());
    for (std::size_t index = 0; index < Memory->target.size(); ++index) {
        Require(Memory->target[index] == ReferenceBytes[index], "byte " + std::to_string(index) + " of the indirect draw whose record the guest reused after the command processor passed it is " + std::to_string(static_cast<unsigned>(Memory->target[index])) + ", the record it was given draws " + std::to_string(static_cast<unsigned>(ReferenceBytes[index])));
    }
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
#ifdef _WIN32
        auto* block = VirtualAlloc(nullptr, GuestBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        auto* block = std::aligned_alloc(GuestBytes, GuestBytes);
#endif
        Require(block != nullptr, "cannot allocate guest memory");
        GuestAllocations::Mutation().Add(block, GuestBytes, true, true, true);
        Memory = new (block) Guest{};
        Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Registered(PixelHeader, PixelCode.data(), sizeof(PixelCode), 1);
        ReferenceDraw();
        ReusedRecordKeepsTheDraw();
        ProducedRecordKeepsTheDraw();
        AgcDriverShutdown_nid_postfix();
        std::puts("a GPU-side indirect draw keeps the record it was given when the guest reuses the record after the command processor passed the draw");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
