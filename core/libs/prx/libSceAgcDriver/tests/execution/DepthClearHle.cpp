#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "DepthFastClearHarness.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;

namespace Graphics = AgcDriver::Graphics;

constexpr std::uint32_t Format16UNorm = 7;
constexpr std::uint32_t Format16Uint = 11;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Format32Uint = 20;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t TileLinear = 0;
constexpr std::uint32_t TileZ64KBX = 0x18;

constexpr std::array<std::uint32_t, 15> ClearKernel{0xd7460004u, 0x0401060cu, 0xf4280204u, 0xfa000000u, 0xd7460005u, 0x0405060du, 0x7e0c020eu, 0xbf8cc07fu, 0x7e000208u, 0x7e020209u, 0x7e04020au, 0x7e06020bu, 0xf0200f28u, 0x00000004u, 0xbf810000u};

class GuestBlock {
public:
    GuestBlock() {
        constexpr std::size_t bytes = 65536;
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(bytes, bytes));
#endif
        Require(block != nullptr, "depth clear: cannot allocate a guest block");
        std::memset(block, 0, bytes);
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() const { return block; }

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

GuestBlock& NewBlock() {
    static auto* blocks = new std::deque<GuestBlock>();
    return blocks->emplace_back();
}

std::uint32_t Low(std::uint64_t value) { return static_cast<std::uint32_t>(value); }

std::uint32_t High(std::uint64_t value) { return static_cast<std::uint32_t>(value >> 32u); }

std::vector<std::uint32_t> Pm4Packet(std::uint32_t opcode, std::initializer_list<std::uint32_t> body) {
    std::vector<std::uint32_t> words{0xc0000000u | (static_cast<std::uint32_t>(body.size() - 1) << 16u) | (opcode << 8u)};
    words.insert(words.end(), body);
    return words;
}

void Submit(std::vector<std::vector<std::uint32_t>> packets) {
    std::vector<std::uint32_t> words;
    for (const auto& packet : packets) words.insert(words.end(), packet.begin(), packet.end());
    Packet dcb{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&dcb) == 0, "depth clear: DCB submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint64_t address, std::uint32_t format, std::uint32_t tile = TileLinear, std::uint32_t type = Type2DArray) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (tile << 20u) | (type << 28u),
        0u, 0u, 0u, 0u,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address, std::uint32_t bytes) {
    return {Low(address), High(address) & 0xffffu, bytes, 0x31016facu};
}

void DispatchClear(const GuestBlock& code, const GuestBlock& plane, const GuestBlock& color, std::uint32_t format, std::uint32_t groupsX, std::uint32_t groupsY, std::uint32_t tile = TileLinear) {
    const auto kernel = code.Address();
    const auto texture = TextureDescriptor(plane.Address(), format, tile);
    const auto buffer = BufferDescriptor(color.Address(), 16u);
    std::vector<std::vector<std::uint32_t>> packets{
        Pm4Packet(0x76, {0x20c, Low(kernel >> 8u)}),
        Pm4Packet(0x76, {0x20d, High(kernel >> 8u) & 0xffu}),
        Pm4Packet(0x76, {0x207, 8}),
        Pm4Packet(0x76, {0x208, 8}),
        Pm4Packet(0x76, {0x209, 1}),
        Pm4Packet(0x76, {0x213, (12u << 1u) | 0x80u | 0x100u | 0x200u | (1u << 11u)}),
    };
    for (std::uint32_t word = 0; word < 8u; ++word) packets.push_back(Pm4Packet(0x76, {0x240u + word, texture[word]}));
    for (std::uint32_t word = 0; word < 4u; ++word) packets.push_back(Pm4Packet(0x76, {0x248u + word, buffer[word]}));
    packets.push_back(Pm4Packet(0x15, {groupsX, groupsY, 1, 0x41}));
    Submit(std::move(packets));
}

void RunClear(AgcDriver::VulkanDevice& device, const GuestBlock& code, const GuestBlock& plane, const GuestBlock& color, std::uint32_t format, std::uint32_t groupsX, std::uint32_t groupsY, std::uint32_t tile = TileLinear) {
    std::vector<std::uint32_t> userData(12);
    const auto texture = TextureDescriptor(plane.Address(), format, tile);
    const auto buffer = BufferDescriptor(color.Address(), 16u);
    std::copy(texture.begin(), texture.end(), userData.begin());
    std::copy(buffer.begin(), buffer.end(), userData.begin() + 8);
    const std::span<const std::uint32_t> kernel(reinterpret_cast<const std::uint32_t*>(code.Data()), ClearKernel.size());
    const ShaderRecompiler::ShaderComputeStageInfo compute{{8, 8, 1}, 0u, {true, true, true}, false, 2u, {0, 0, 0}};
    const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{
        {static_cast<std::uintptr_t>(code.Address()), std::as_bytes(kernel)},
        {static_cast<std::uintptr_t>(color.Address()), std::span<const std::byte>(reinterpret_cast<const std::byte*>(color.Data()), 16)},
    }};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, static_cast<std::uintptr_t>(code.Address()), kernel, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto program = ShaderRecompiler::Recompile(request);
    std::optional<Graphics::DepthPlaneClear> scope;
    if (const auto clear = Graphics::MatchStorageImageClear(kernel, userData, compute, {groupsX, groupsY, 1u})) scope.emplace(*clear);
    device.Dispatch(program, groupsX, groupsY, 1, {}, static_cast<std::uintptr_t>(code.Address()));
    scope.reset();
    device.WaitIdle();
}

void SetColor(GuestBlock& color, float value) {
    const std::array<float, 4> channels{value, value, value, value};
    std::memcpy(color.Data(), channels.data(), sizeof(channels));
    AgcDriver::GuestMemory::MarkWritten(color.Address(), sizeof(channels));
}

void SetRaw(GuestBlock& color, std::uint32_t raw) {
    const std::array<std::uint32_t, 4> channels{raw, 0u, 0u, 0u};
    std::memcpy(color.Data(), channels.data(), sizeof(channels));
    AgcDriver::GuestMemory::MarkWritten(color.Address(), sizeof(channels));
}

Graphics::DepthClearStatistics Difference(const Graphics::DepthClearStatistics& now, const Graphics::DepthClearStatistics& before) {
    Graphics::DepthClearStatistics difference;
    difference.matched = now.matched - before.matched;
    difference.taken = now.taken - before.taken;
    difference.copiesSkipped = now.copiesSkipped - before.copiesSkipped;
    difference.clears = now.clears - before.clears;
    difference.bytesSkipped = now.bytesSkipped - before.bytesSkipped;
    for (std::size_t reason = 0; reason < difference.declined.size(); ++reason) difference.declined[reason] = now.declined[reason] - before.declined[reason];
    return difference;
}

std::uint64_t Declined(const Graphics::DepthClearStatistics& statistics) {
    std::uint64_t total = 0;
    for (const auto count : statistics.declined) total += count;
    return total;
}

void MatcherCases() {
    std::vector<std::uint32_t> userData(12);
    for (std::uint32_t word = 0; word < userData.size(); ++word) userData[word] = 0x1000u + word;
    ShaderRecompiler::ShaderComputeStageInfo compute{{8, 8, 1}, 0u, {true, true, true}, false, 2u, {0, 0, 0}};
    const std::array<std::uint32_t, 3> groups{8, 4, 1};
    std::vector<std::uint32_t> code(ClearKernel.begin(), ClearKernel.end());
    code.insert(code.end(), {0u, 0u, 0u, 0u});

    const auto clear = Graphics::MatchStorageImageClear(code, userData, compute, groups);
    Require(clear.has_value(), "the clear kernel was not recognized");
    Require(clear->width == 64u && clear->height == 32u, "the clear covers " + std::to_string(clear->width) + " x " + std::to_string(clear->height) + " texels, expected 64 x 32");
    for (std::uint32_t word = 0; word < 8u; ++word) Require(clear->image[word] == 0x1000u + word, "the clear's T# is not user data 0-7");
    for (std::uint32_t word = 0; word < 4u; ++word) Require(clear->color[word] == 0x1008u + word, "the clear's colour V# is not user data 8-11");

    for (std::size_t word = 0; word < ClearKernel.size(); ++word) {
        auto changed = code;
        changed[word] ^= 0x00000100u;
        Require(!Graphics::MatchStorageImageClear(changed, userData, compute, groups), "a kernel with word " + std::to_string(word) + " changed was recognized");
    }
    Require(!Graphics::MatchStorageImageClear(std::span(code).first(ClearKernel.size() - 1), userData, compute, groups), "a truncated kernel was recognized");
    Require(Graphics::MatchStorageImageClear(std::span(code).first(ClearKernel.size()), userData, compute, groups).has_value(), "the kernel alone, without trailing words, was not recognized");

    auto shortUserData = userData;
    shortUserData.pop_back();
    Require(!Graphics::MatchStorageImageClear(code, shortUserData, compute, groups), "eleven user registers were accepted (s12 would be a user register, not the group id)");
    auto longUserData = userData;
    longUserData.push_back(0u);
    Require(!Graphics::MatchStorageImageClear(code, longUserData, compute, groups), "thirteen user registers were accepted (s12 would be a user register, not the group id)");

    auto wide = compute;
    wide.numThreads = {8, 8, 2};
    Require(!Graphics::MatchStorageImageClear(code, userData, wide, groups), "8 x 8 x 2 threads were accepted");
    wide.numThreads = {16, 4, 1};
    Require(!Graphics::MatchStorageImageClear(code, userData, wide, groups), "16 x 4 threads were accepted");
    for (std::size_t axis = 0; axis < 3u; ++axis) {
        auto missing = compute;
        missing.groupIdEnable[axis] = false;
        Require(!Graphics::MatchStorageImageClear(code, userData, missing, groups), "a dispatch without the group id of axis " + std::to_string(axis) + " was accepted");
    }
    auto oneComponent = compute;
    oneComponent.threadIdComponentCount = 1u;
    Require(!Graphics::MatchStorageImageClear(code, userData, oneComponent, groups), "a dispatch with only the x thread id was accepted");

    Require(!Graphics::MatchStorageImageClear(code, userData, compute, {8, 4, 2}), "two layers of groups were accepted");
    Require(!Graphics::MatchStorageImageClear(code, userData, compute, {0, 4, 1}), "zero groups along x were accepted");
    Require(!Graphics::MatchStorageImageClear(code, userData, compute, {8, 0, 1}), "zero groups along y were accepted");

    auto partial = compute;
    partial.partialThreads = {60, 30, 1};
    const auto cut = Graphics::MatchStorageImageClear(code, userData, partial, groups);
    Require(cut.has_value() && cut->width == 60u && cut->height == 30u, "a dispatch of 60 x 30 threads does not cover 60 x 30 texels");

}

std::vector<std::uint32_t> DepthImageTexels(AgcDriver::VulkanDevice& device, const GuestBlock& plane, std::uint32_t format, std::uint32_t texelBytes) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto words = TextureDescriptor(plane.Address(), format);
    const auto resource = Graphics::DecodeTextureResource(words);
    std::shared_ptr<Graphics::StorageTexture> mirror;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        mirror = Graphics::DepthSurfaceStorage(context, words, resource, 0);
    }
    Require(mirror != nullptr, "the plane has no mirror");
    mirror->MarkDirty();
    const std::size_t pitch = std::max<std::size_t>(Width, 256u / texelBytes);
    Graphics::StorageTexture::FlushPending(plane.Address(), pitch * Height * texelBytes, nullptr, "test");
    device.WaitIdle();
    std::vector<std::uint32_t> texels(static_cast<std::size_t>(Width) * Height);
    for (std::size_t y = 0; y < Height; ++y) {
        for (std::size_t x = 0; x < Width; ++x) {
            const auto at = (y * pitch + x) * texelBytes;
            if (texelBytes == 2u) {
                std::uint16_t value = 0;
                std::memcpy(&value, plane.Data() + at, sizeof(value));
                texels[y * Width + x] = value;
            } else {
                std::memcpy(&texels[y * Width + x], plane.Data() + at, sizeof(std::uint32_t));
            }
        }
    }
    return texels;
}

void RequireDepthTexels(AgcDriver::VulkanDevice& device, const GuestBlock& plane, std::uint32_t format, std::uint32_t texelBytes, std::uint32_t expected, const std::string& label) {
    const auto texels = DepthImageTexels(device, plane, format, texelBytes);
    for (std::size_t texel = 0; texel < texels.size(); ++texel) Require(texels[texel] == expected, label + "depth image texel " + std::to_string(texel) + " holds " + std::to_string(texels[texel]) + ", expected " + std::to_string(expected));
}

Graphics::DepthClearStatistics ClearSequence(AgcDriver::VulkanDevice& device, const Shaders& shaders, int mode) {
    Graphics::SetDepthPlaneClearForTest(mode != 0);
    const auto before = Graphics::DepthClearCounts();
    const std::string label = "mode " + std::to_string(mode) + ": ";
    auto& code = NewBlock();
    std::memcpy(code.Data(), ClearKernel.data(), sizeof(ClearKernel));
    auto& plane = NewBlock();
    auto& color = NewBlock();
    const Surface surface{plane.Address(), 0, 0, false, VK_FORMAT_D32_SFLOAT};
    const auto drawLess = [&] { return Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_LESS}); };
    const auto drawGreater = [&] { return Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_GREATER}); };
    const auto drawEqual = [&] { return Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_EQUAL}); };

    Require(Draw(device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, label + "the producer did not cover the plane");
    Require(drawLess() == 0u, label + "the plane does not hold the 0.5 the producer wrote");
    Require(drawEqual() == Covered, label + "the plane does not hold exactly 0.5");

    SetColor(color, 1.0f);
    RunClear(device, code, plane, color, Format32Float, 8, 4);
    Require(drawLess() == Covered, label + "after a clear to 1.0 the depth test 0.5 < 1.0 did not pass everywhere");
    Require(drawGreater() == 0u, label + "after a clear to 1.0 a texel passed 0.5 > depth");

    SetColor(color, 0.25f);
    RunClear(device, code, plane, color, Format32Float, 8, 4);
    Require(drawLess() == 0u, label + "after a clear to 0.25 a texel passed 0.5 < depth");
    Require(drawGreater() == Covered, label + "after a clear to 0.25 the depth test 0.5 > 0.25 did not pass everywhere");

    SetColor(color, 0.5f);
    RunClear(device, code, plane, color, Format32Float, 8, 4);
    Require(drawEqual() == Covered, label + "after a clear to 0.5 the plane does not hold exactly 0.5");

    SetColor(color, 0.75f);
    RunClear(device, code, plane, color, Format32Float, 4, 4);
    Require(drawGreater() == 0u, label + "after a clear of the left half to 0.75 a texel passed 0.5 > depth");
    Require(drawEqual() == Covered / 2u, label + "after a clear of the left half the right half does not keep 0.5");
    Require(drawLess() == Covered / 2u, label + "after a clear of the left half the left half does not hold 0.75");

    SetColor(color, 2.0f);
    RunClear(device, code, plane, color, Format32Float, 8, 4);
    Require(drawEqual() == 0u, label + "after a clear to 2.0 a texel still holds exactly 0.5");

    SetColor(color, 0.125f);
    RunClear(device, code, plane, color, Format32Float, 8, 4);
    Require(drawGreater() == Covered, label + "after the last full clear the depth test 0.5 > 0.125 did not pass everywhere");
    Require(drawLess() == 0u, label + "after the last full clear a texel passed 0.5 < depth");

    Graphics::StorageTexture::FlushPending(plane.Address(), Width * Height * sizeof(float), nullptr, "test");
    device.WaitIdle();
    for (std::uint32_t texel = 0; texel < Width * Height; ++texel) {
        float value = 0.0f;
        std::memcpy(&value, plane.Data() + texel * sizeof(float), sizeof(value));
        Require(value == 0.125f, label + "guest depth texel " + std::to_string(texel) + " is " + std::to_string(value) + " after the flush, expected 0.125");
    }
    RequireDepthTexels(device, plane, Format32Float, 4, std::bit_cast<std::uint32_t>(0.125f), label);
    return Difference(Graphics::DepthClearCounts(), before);
}

void IntegerViewCase(AgcDriver::VulkanDevice& device, const Shaders& shaders, int mode) {
    Graphics::SetDepthPlaneClearForTest(mode != 0);
    const auto before = Graphics::DepthClearCounts();
    const std::string label = "integer view, mode " + std::to_string(mode) + ": ";
    auto& code = NewBlock();
    std::memcpy(code.Data(), ClearKernel.data(), sizeof(ClearKernel));
    auto& plane = NewBlock();
    auto& color = NewBlock();
    const Surface surface{plane.Address(), 0, 0, false, VK_FORMAT_D32_SFLOAT};
    Require(Draw(device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, label + "the producer did not cover the plane");
    const std::array<std::uint32_t, 4> bits{std::bit_cast<std::uint32_t>(0.25f), 0u, 0u, 0u};
    std::memcpy(color.Data(), bits.data(), sizeof(bits));
    AgcDriver::GuestMemory::MarkWritten(color.Address(), sizeof(bits));
    RunClear(device, code, plane, color, Format32Uint, 8, 4, TileZ64KBX);
    Require(Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_GREATER}) == Covered, label + "after a clear to the bits of 0.25 the depth test 0.5 > 0.25 did not pass everywhere");
    Require(Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_LESS}) == 0u, label + "after a clear to the bits of 0.25 a texel passed 0.5 < depth");
    const auto difference = Difference(Graphics::DepthClearCounts(), before);
    if (mode == 0) Require(difference.matched == 0u && difference.clears == 0u, label + "the lever counted with the switch off");
    else Require(difference.clears == 1u, label + "the clear was not taken (" + std::to_string(difference.clears) + " clears, " + std::to_string(Declined(difference)) + " declined)");
}

void DriverMatchCase(AgcDriver::VulkanDevice& device, const Shaders& shaders, int mode) {
    Graphics::SetDepthPlaneClearForTest(mode != 0);
    const auto before = Graphics::DepthClearCounts();
    const std::string label = "driver submit, mode " + std::to_string(mode) + ": ";
    auto& code = NewBlock();
    std::memcpy(code.Data(), ClearKernel.data(), sizeof(ClearKernel));
    auto& plane = NewBlock();
    auto& color = NewBlock();
    const Surface surface{plane.Address(), 0, 0, false, VK_FORMAT_D32_SFLOAT};
    Require(Draw(device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, label + "the producer did not cover the plane");
    SetColor(color, 1.0f);
    DispatchClear(code, plane, color, Format32Float, 8, 4);
    const auto difference = Difference(Graphics::DepthClearCounts(), before);
    if (mode == 0) Require(difference.matched == 0u && difference.taken == 0u, label + "the driver's dispatch reached the lever with the switch off");
    else Require(difference.matched == 1u && difference.taken == 1u, label + "the driver's dispatch matched " + std::to_string(difference.matched) + " and took " + std::to_string(difference.taken) + " clears (declined " + std::to_string(Declined(difference)) + "), expected 1 and 1");
}

void SixteenBitCase(AgcDriver::VulkanDevice& device, const Shaders& shaders, int mode) {
    Graphics::SetDepthPlaneClearForTest(mode != 0);
    const auto before = Graphics::DepthClearCounts();
    const std::string label = "16-bit plane, mode " + std::to_string(mode) + ": ";
    auto& code = NewBlock();
    std::memcpy(code.Data(), ClearKernel.data(), sizeof(ClearKernel));
    auto& plane = NewBlock();
    auto& color = NewBlock();
    const Surface surface{plane.Address(), 0, 0, false, VK_FORMAT_D16_UNORM};
    const auto drawLess = [&] { return Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_LESS}); };
    const auto drawGreater = [&] { return Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_GREATER}); };
    Require(Draw(device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, label + "the producer did not cover the plane");

    SetColor(color, 1.0f);
    try {
        RunClear(device, code, plane, color, Format16UNorm, 8, 4);
    } catch (const std::exception& error) {
        if (std::string(error.what()).find("unsupported format") == std::string::npos) throw;
        std::printf("16-bit plane skipped: %s\n", error.what());
        return;
    }
    Require(drawLess() == Covered, label + "after a clear to 1.0 the depth test 0.5 < 1.0 did not pass everywhere");
    Require(drawGreater() == 0u, label + "after a clear to 1.0 a texel passed 0.5 > depth");
    RequireDepthTexels(device, plane, Format16UNorm, 2, 0xffffu, label + "clear to 1.0: ");

    SetColor(color, 0.0f);
    RunClear(device, code, plane, color, Format16UNorm, 8, 4);
    Require(drawGreater() == Covered, label + "after a clear to 0.0 the depth test 0.5 > 0.0 did not pass everywhere");
    Require(drawLess() == 0u, label + "after a clear to 0.0 a texel passed 0.5 < depth");
    RequireDepthTexels(device, plane, Format16UNorm, 2, 0u, label + "clear to 0.0: ");

    SetColor(color, 0.5f);
    RunClear(device, code, plane, color, Format16UNorm, 8, 4);

    auto& rawPlane = NewBlock();
    const Surface rawSurface{rawPlane.Address(), 0, 0, false, VK_FORMAT_D16_UNORM};
    const auto rawLess = [&] { return Draw(device, shaders, rawSurface, {.depthCompare = VK_COMPARE_OP_LESS}); };
    const auto rawGreater = [&] { return Draw(device, shaders, rawSurface, {.depthCompare = VK_COMPARE_OP_GREATER}); };
    Require(Draw(device, shaders, rawSurface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, label + "the producer of the uint plane did not cover it");
    SetRaw(color, 0x4000u);
    RunClear(device, code, rawPlane, color, Format16Uint, 8, 4, TileZ64KBX);
    Require(rawGreater() == Covered, label + "after a clear to the raw value 0x4000 the depth test 0.5 > 0.25 did not pass everywhere");
    Require(rawLess() == 0u, label + "after a clear to the raw value 0x4000 a texel passed 0.5 < depth");

    SetRaw(color, 0xffffu);
    RunClear(device, code, rawPlane, color, Format16Uint, 8, 4, TileZ64KBX);
    Require(rawLess() == Covered, label + "after a clear to the raw value 0xffff the depth test 0.5 < 1.0 did not pass everywhere");
    Require(rawGreater() == 0u, label + "after a clear to the raw value 0xffff a texel passed 0.5 > depth");

    SetRaw(color, 0u);
    RunClear(device, code, rawPlane, color, Format16Uint, 8, 4, TileZ64KBX);
    Require(rawGreater() == Covered, label + "after a clear to the raw value 0 the depth test 0.5 > 0.0 did not pass everywhere");
    Require(rawLess() == 0u, label + "after a clear to the raw value 0 a texel passed 0.5 < depth");

    const auto difference = Difference(Graphics::DepthClearCounts(), before);
    if (mode == 0) {
        Require(difference.matched == 0u && difference.clears == 0u, label + "the lever counted with the switch off");
        return;
    }
    Require(difference.clears == 4u, label + "the lever took " + std::to_string(difference.clears) + " clears, expected 4 (unorm 1.0 and 0.0, raw 0xffff and 0)");
    Require(difference.declined[2] >= 2u, label + "the unorm clear to 0.5 and the raw clear to 0x4000 were not declined for their value (" + std::to_string(difference.declined[2]) + ")");
    Require(difference.bytesSkipped == difference.clears * 2u * Width * Height * sizeof(std::uint16_t), label + "the copies left out add up to " + std::to_string(difference.bytesSkipped) + " bytes");
}

void StencilCase(AgcDriver::VulkanDevice& device, const Shaders& shaders, int mode) {
    Graphics::SetDepthPlaneClearForTest(mode != 0);
    const auto before = Graphics::DepthClearCounts();
    const std::string label = "depth and stencil, mode " + std::to_string(mode) + ": ";
    auto& code = NewBlock();
    std::memcpy(code.Data(), ClearKernel.data(), sizeof(ClearKernel));
    auto& plane = NewBlock();
    auto& stencilPlane = NewBlock();
    auto& color = NewBlock();
    const Surface surface{plane.Address(), stencilPlane.Address(), 0, false, VK_FORMAT_D32_SFLOAT_S8_UINT};
    const VkStencilOpState writeFive{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 5u};
    const VkStencilOpState equalFive{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, 5u};
    Require(Draw(device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .stencilTest = true, .stencil = writeFive}) == Covered, label + "the depth and stencil producer did not cover the surface");
    SetColor(color, 1.0f);
    RunClear(device, code, plane, color, Format32Float, 8, 4);
    Require(Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_LESS}) == Covered, label + "after a clear to 1.0 the depth test 0.5 < 1.0 did not pass everywhere");
    Require(Draw(device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = equalFive}) == Covered, label + "the depth clear changed the stencil values");
    const auto difference = Difference(Graphics::DepthClearCounts(), before);
    if (mode == 0) Require(difference.matched == 0u && difference.clears == 0u, label + "the lever counted with the switch off");
    else Require(difference.clears == 1u, label + "the clear was not taken (" + std::to_string(difference.clears) + " clears, " + std::to_string(Declined(difference)) + " declined)");
}

}

int main() {
    int failures = 0;
    const auto run = [&](const char* name, auto&& body) {
        try {
            body();
        } catch (const std::exception& error) {
            std::cerr << name << ": " << error.what() << '\n';
            ++failures;
        }
    };
    try {
        run("matcher", [] { MatcherCases(); });
        const auto device = OpenVulkanTestDevice();
        if (!device) return failures != 0 ? 1 : VulkanTestSkipped;
        const auto shaders = Compile(*device);

        run("switch off", [&] {
            const auto off = ClearSequence(*device, shaders, 0);
            Require(off.matched == 0u && off.taken == 0u && off.clears == 0u && off.copiesSkipped == 0u, "the lever counted with the switch off");
        });

        run("lever", [&] {
            const auto on = ClearSequence(*device, shaders, 1);
            Require(on.matched >= 6u, "the lever matched " + std::to_string(on.matched) + " clear dispatches, expected 6");
            Require(on.clears == 4u, "the lever took " + std::to_string(on.clears) + " clears, expected 4 (1.0, 0.25, 0.5, 0.125)");
            Require(on.copiesSkipped >= on.clears, "fewer copy-ins were left out than clears taken");
            Require(on.bytesSkipped == on.clears * 2u * Width * Height * sizeof(float), "the copies left out add up to " + std::to_string(on.bytesSkipped) + " bytes");
            Require(on.declined[2] >= 1u, "the clear to 2.0 was not declined for its value (" + std::to_string(on.declined[2]) + ")");
            Require(on.declined[5] >= 1u, "the clear of half the plane was not declined for its coverage (" + std::to_string(on.declined[5]) + ")");
        });

        Graphics::SetDepthPlaneClearForTest(true);

        for (const int mode : {0, 1}) run(mode == 0 ? "integer view, switch off" : "integer view, lever", [&] { IntegerViewCase(*device, shaders, mode); });
        for (const int mode : {0, 1}) run(mode == 0 ? "16-bit plane, switch off" : "16-bit plane, lever", [&] { SixteenBitCase(*device, shaders, mode); });
        for (const int mode : {0, 1}) run(mode == 0 ? "depth and stencil, switch off" : "depth and stencil, lever", [&] { StencilCase(*device, shaders, mode); });
        for (const int mode : {0, 1}) run(mode == 0 ? "driver submit, switch off" : "driver submit, lever", [&] { DriverMatchCase(*device, shaders, mode); });
        Graphics::SetDepthPlaneClearForTest(true);
        LibcRunShutdown_nid_postfix();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
    if (failures != 0) return 1;
    std::puts("depth clear tests passed");
    return 0;
}
