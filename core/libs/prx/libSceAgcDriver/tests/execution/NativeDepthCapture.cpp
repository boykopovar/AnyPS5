#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/NativeDepthCapture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "DepthFastClearHarness.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>

namespace AgcDriver {
struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};
}
namespace AgcDriver::DriverDetail {
struct DriverTestAccess {
    static void UseDevice(std::shared_ptr<VulkanDevice> value) { Driver::Get().device = std::move(value); }
    static void Stop() { Driver::Get().stop(); }
};
}

namespace {

using DepthFastClearHarness::Require;
using AgcDriver::NativeDepthSnapshot;
constexpr std::size_t Step = 131072u;
constexpr std::uint32_t Width = DepthFastClearHarness::Width;
constexpr std::uint32_t Height = DepthFastClearHarness::Height;

std::uintptr_t Allocate() {
    constexpr std::size_t bytes = 2u << 20u;
    auto* memory = GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536u);
    Require(memory != nullptr, "native depth fixture cannot allocate guest memory");
    GuestArena::GuestArenaCommit_nid_postfix(memory, bytes, 0x04, bytes);
    GuestAllocations::Mutation().Add(memory, bytes, true, true, true);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(memory, bytes);
    return reinterpret_cast<std::uintptr_t>(memory);
}

DepthFastClearHarness::Surface Surface(std::uintptr_t base, unsigned index) {
    return {base + index * Step, index == 6u || index == 7u ? base + 12u * Step : 0u, 0u, false,
        index == 6u || index == 7u ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_D16_UNORM, index, 0u, 0u};
}

float Value(unsigned index) { return index == 6u ? 0.25f : index == 7u ? 0.75f : static_cast<float>(index + 1u) / 8.0f; }

void Render(AgcDriver::VulkanDevice& device, std::uintptr_t base) {
    const auto shaders = DepthFastClearHarness::Compile(device);
    for (unsigned index = 0; index < 8u; ++index) {
        DepthFastClearHarness::DrawOptions options;
        options.depthWrite = true;
        options.depthCompare = VK_COMPARE_OP_ALWAYS;
        options.minDepth = options.maxDepth = Value(index);
        options.stencilTest = index >= 6u;
        options.stencil = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 255u, 255u, index == 6u ? 37u : 203u};
        Require(DepthFastClearHarness::Draw(device, shaders, Surface(base, index), options) == Width * Height, "native depth source draw did not cover its plane");
    }
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const AgcDriver::Graphics::DepthTarget clear{base + 8u * Step, 0u, {Width, Height}, VK_FORMAT_D16_UNORM, 0.875f, 0u};
    static_cast<void>(AgcDriver::Graphics::DepthSurfaceView(context, clear, false, false));
    auto retired = clear;
    retired.address = base + 9u * Step;
    static_cast<void>(AgcDriver::Graphics::DepthSurfaceView(context, retired, false, false));
    device.WaitIdle();
    AgcDriver::Graphics::RetireDepthSurfaces(device.Device(), retired.address, Step);
}

void Check(std::span<const NativeDepthSnapshot> snapshots, std::uintptr_t base) {
    Require(snapshots.size() == 9u, "native capture lost a modeled slice or included a retired owner");
    constexpr std::array<std::uint16_t, 9> expected{0x2000u, 0x4000u, 0x6000u, 0x8000u, 0x9fffu, 0xbfffu, 0u, 0u, 0xdfffu};
    std::array<bool, 9> seen{};
    for (const auto& snapshot : snapshots) {
        Require(snapshot.target.address >= base && (snapshot.target.address - base) % Step == 0u, "native snapshot has an unrelated address");
        const auto index = static_cast<unsigned>((snapshot.target.address - base) / Step);
        Require(index < seen.size() && !seen[index], "native snapshot duplicated a plane");
        seen[index] = true;
        const bool d16 = index < 6u || index == 8u;
        Require(snapshot.depth.size() == Width * Height * (d16 ? 2u : 4u), "native depth snapshot has the wrong size");
        for (std::size_t pixel = 0; pixel < Width * Height; ++pixel) {
            if (d16) {
                std::uint16_t value = 0;
                std::memcpy(&value, snapshot.depth.data() + pixel * 2u, 2u);
                Require(std::abs(static_cast<int>(value) - expected[index]) <= 1, "captured D16 depth differs from the rendered viewport depth");
            } else {
                std::uint32_t value = 0;
                std::memcpy(&value, snapshot.depth.data() + pixel * 4u, 4u);
                Require(value == std::bit_cast<std::uint32_t>(Value(index)), "captured D32 depth differs from the rendered viewport depth");
            }
        }
        if (index == 7u) Require(snapshot.stencil.size() == Width * Height && std::ranges::all_of(snapshot.stencil, [](std::byte value) { return value == std::byte{203}; }), "capture lost the current shared stencil owner");
        else Require(snapshot.stencil.empty(), "capture retained a stale stencil owner");
    }
}

void Sample(AgcDriver::VulkanDevice& device, std::uintptr_t address, float expected, std::uint32_t format = 7u) {
    alignas(256) static std::array<std::uint32_t, 32> output{};
    constexpr std::array<std::uint32_t, 11> code{0x7e02020du, 0x7e04020cu, 0xf0001108u, 0x00010300u, 0xbf8c3f70u, 0x34080082u, 0xe0701000u, 0x80000304u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
    const auto out = reinterpret_cast<std::uintptr_t>(output.data());
    std::array<std::uint32_t, 16> user{};
    const std::array<std::uint32_t, 4> buffer{static_cast<std::uint32_t>(out), static_cast<std::uint32_t>(out >> 32u), sizeof(output), 0x31016facu};
    const std::array<std::uint32_t, 8> image{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 255u) | (format << 20u) | (((Width - 1u) & 3u) << 30u), ((Width - 1u) >> 2u) | ((Height - 1u) << 14u), 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
    std::copy(buffer.begin(), buffer.end(), user.begin());
    std::copy(image.begin(), image.end(), user.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}}, {32u, 0u, user, compute, std::nullopt, std::nullopt, memory}, device.Target(), {0u, 0u, 0u, 128u}};
    request.useCache = false;
    output.fill(0xdeadbeefu);
    const auto shader = ShaderRecompiler::Recompile(request);
    device.Dispatch(shader, 1u, 1u, 1u, {}, request.shader.codeAddress);
    device.WaitIdle();
    for (const auto value : output) Require(format == 5u ? value == static_cast<std::uint32_t>(expected) : std::abs(std::bit_cast<float>(value) - expected) < 2.0f / 65535.0f, "sampling before any replayed draw did not see the restored native plane");
}

void RoundTrip() {
    const auto base = Allocate();
    std::vector<NativeDepthSnapshot> snapshots;
    {
        auto source = OpenVulkanTestDevice();
        if (!source) throw std::runtime_error("native depth source device unavailable");
        Render(*source, base);
        snapshots = source->CaptureNativeDepth();
        Check(snapshots, base);
    }
    auto target = OpenVulkanTestDevice();
    Require(target != nullptr, "native depth target device unavailable");
    auto invalid = snapshots;
    invalid.back().hostFormat = VK_FORMAT_UNDEFINED;
    bool refused = false;
    try { target->RestoreNativeDepth(invalid); } catch (const std::runtime_error&) { refused = true; }
    Require(refused && target->CaptureNativeDepth().empty(), "invalid restore changed native state before validating all records");
    auto restricted = AgcDriver::VulkanDeviceTestAccess::ContextOf(*target);
    restricted.depthRangeUnrestricted = false;
    for (const auto bits : {0x7fc00000u, 0x3fc00000u}) {
        auto invalidDepth = snapshots;
        std::memcpy(invalidDepth[6].depth.data(), &bits, sizeof(bits));
        refused = false;
        try { AgcDriver::Graphics::RestoreNativeDepthSurfaces(restricted, invalidDepth); } catch (const std::runtime_error&) { refused = true; }
        Require(refused && target->CaptureNativeDepth().empty(), "restricted depth restoration accepted an invalid depth value or changed state before refusing it");
    }
    target->RestoreNativeDepth(snapshots);
    const auto restored = target->CaptureNativeDepth();
    Check(restored, base);
    for (const auto& original : snapshots) {
        const auto found = std::ranges::find_if(restored, [&](const auto& value) { return value.target.address == original.target.address; });
        Require(found != restored.end() && found->depth == original.depth && found->stencil == original.stencil, "native depth/stencil bits changed across restoration");
    }
    for (unsigned index = 0; index < 6u; ++index) Sample(*target, base + index * Step, Value(index));
    Sample(*target, base + 8u * Step, 0.875f);
    const auto shaders = DepthFastClearHarness::Compile(*target);
    DepthFastClearHarness::DrawOptions options;
    options.depthCompare = VK_COMPARE_OP_EQUAL;
    options.minDepth = options.maxDepth = Value(7u);
    options.stencilTest = true;
    options.stencil = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 255u, 0u, 203u};
    Require(DepthFastClearHarness::Draw(*target, shaders, Surface(base, 7u), options) == Width * Height, "the first draw lost restored depth or the current stencil owner");
}

void MirrorableStencil() {
    const auto base = Allocate();
    bool sourcePublished = false;
    const auto stencil = base + 12u * Step;
    const auto swizzle = AgcDriver::Graphics::XorSwizzleMode(AgcDriver::Graphics::TextureTileMode::kZ64KBX);
    std::vector<NativeDepthSnapshot> snapshots;
    {
        auto source = OpenVulkanTestDevice();
        Require(source != nullptr, "mirrorable stencil source unavailable");
        const auto shaders = DepthFastClearHarness::Compile(*source);
        for (const auto index : {1u, 0u}) {
            const DepthFastClearHarness::Surface surface{base + index * Step, stencil, 0u, false, VK_FORMAT_D16_UNORM_S8_UINT, 0u, 0u, swizzle};
            DepthFastClearHarness::DrawOptions options;
            options.depthWrite = true;
            options.depthCompare = VK_COMPARE_OP_ALWAYS;
            options.stencilTest = true;
            options.stencil = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 255u, 255u, index == 0u ? 203u : 37u};
            Require(DepthFastClearHarness::Draw(*source, shaders, surface, options) == Width * Height, "mirrorable stencil source draw failed");
        }
        snapshots = source->CaptureNativeDepth();
        Require(snapshots.size() == 2u && snapshots[0].stencil.size() == Width * Height && snapshots[1].stencil.empty(), "mirrorable stencil capture lost current ownership");
        Require(std::ranges::all_of(snapshots[0].stencil, [](std::byte value) { return value == std::byte{203}; }), "source stencil texels differ from the final draw");
        const auto flushed = AgcDriver::Graphics::FlushDepthPlanes(stencil, Step);
        source->WaitIdle();
        std::vector<std::byte> bytes(AgcDriver::Graphics::DepthSliceBytes({Width, Height}, 1u));
        AgcDriver::GuestMemory::ReadCommitted(stencil, bytes);
        sourcePublished = flushed.size() == 1u && std::ranges::count(bytes, std::byte{203}) >= Width * Height;

    }
    auto target = OpenVulkanTestDevice();
    Require(target != nullptr, "mirrorable stencil target unavailable");
    target->RestoreNativeDepth(snapshots);
    const auto flushed = AgcDriver::Graphics::FlushDepthPlanes(stencil, Step);
    target->WaitIdle();
    std::vector<std::byte> bytes(AgcDriver::Graphics::DepthSliceBytes({Width, Height}, 1u));
    AgcDriver::GuestMemory::ReadCommitted(stencil, bytes);
    Require(sourcePublished && flushed.size() == 1u && std::ranges::count(bytes, std::byte{203}) >= Width * Height, "a nonowner stencil replica was published at capture or restoration");
}

void CrossAspect() {
    const auto base = Allocate();
    const auto depth = base + 2u * Step;
    std::vector<NativeDepthSnapshot> snapshots;
    {
        auto source = OpenVulkanTestDevice();
        Require(source != nullptr, "cross-aspect source unavailable");
        const auto shaders = DepthFastClearHarness::Compile(*source);
        DepthFastClearHarness::DrawOptions options;
        options.depthWrite = true;
        options.depthCompare = VK_COMPARE_OP_ALWAYS;
        Require(DepthFastClearHarness::Draw(*source, shaders, {depth, 0u, 0u, false, VK_FORMAT_D16_UNORM}, options) == Width * Height, "cross-aspect depth draw failed");
        options.stencilTest = true;
        options.stencil = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 255u, 255u, 203u};
        Require(DepthFastClearHarness::Draw(*source, shaders, {base + Step, depth, 0u, false, VK_FORMAT_D16_UNORM_S8_UINT}, options) == Width * Height, "cross-aspect stencil draw failed");
        Sample(*source, depth, 203.0f, 5u);
        snapshots = source->CaptureNativeDepth();
        Require(snapshots.size() == 2u && !snapshots[0].stencil.empty(), "cross-aspect capture lost the newer stencil owner");
    }
    auto target = OpenVulkanTestDevice();
    Require(target != nullptr, "cross-aspect target unavailable");
    target->RestoreNativeDepth(snapshots);
    Sample(*target, depth, 203.0f, 5u);
}

void Preflight() {
    const auto base = Allocate();
    auto target = OpenVulkanTestDevice();
    Require(target != nullptr, "native preflight device unavailable");
    NativeDepthSnapshot valid;
    valid.target = {base, 0u, {Width, Height}, VK_FORMAT_D16_UNORM, 1.0f, 0u};
    valid.hostFormat = VK_FORMAT_D16_UNORM;
    valid.depthOrder = 1u;
    valid.depth.resize(Width * Height * 2u);
    auto Refuses = [&](std::span<const NativeDepthSnapshot> snapshots) {
        bool refused = false;
        try { target->RestoreNativeDepth(snapshots); } catch (const std::runtime_error&) { refused = true; }
        Require(refused && target->CaptureNativeDepth().empty(), "invalid native ownership graph or range changed state before preflight refused it");
    };
    auto missing = valid;
    missing.target.format = missing.hostFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
    missing.target.stencilAddress = base + 2u * Step;
    missing.depth.resize(Width * Height * 4u);
    Refuses(std::span(&missing, 1));
    auto aspect = valid;
    aspect.target.stencilAddress = base + 2u * Step;
    Refuses(std::span(&aspect, 1));
    auto overflow = valid;
    overflow.target.address = std::numeric_limits<std::uint64_t>::max() - 1u;
    overflow.depthOrder = 2u;
    const std::array invalidRange{valid, overflow};
    Refuses(invalidRange);
    auto stencilOnly = missing;
    stencilOnly.depth.clear();
    stencilOnly.depthOrder = 0u;
    stencilOnly.stencilOrder = 1u;
    stencilOnly.stencil.assign(Width * Height, std::byte{203});
    Refuses(std::span(&stencilOnly, 1));
    auto companion = stencilOnly;
    companion.target.format = VK_FORMAT_D16_UNORM_S8_UINT;
    companion.hostFormat = VK_FORMAT_D24_UNORM_S8_UINT;
    companion.stencilOrder = 2u;
    const std::array incompatible{missing, companion};
    Refuses(incompatible);
}

void Capture(const std::filesystem::path& tool) {
    const auto* directory = std::getenv("APS5_FRAME_CAPTURE");
    Require(directory != nullptr, "native depth capture directory is missing");
    auto opened = OpenVulkanTestDevice();
    Require(opened != nullptr, "native depth capture device unavailable");
    auto device = std::shared_ptr<AgcDriver::VulkanDevice>(std::move(opened));
    AgcDriver::DriverDetail::DriverTestAccess::UseDevice(device);
    const auto base = Allocate();
    Render(*device, base);
    auto* recorder = AgcDriver::FrameRecorder::Active();
    Require(recorder != nullptr, "native depth recorder is disabled");
    recorder->FrameBoundary();
    recorder->FrameBoundary();
    Check(AgcDriver::ReadNativeDepthCapture(std::filesystem::path(directory) / AgcDriver::NativeDepthCaptureName), base);
    const AgcDriver::FrameCaptureReader capture(directory);
    Require(!capture.Truncated() && capture.Header().find("\"native_depth_surfaces\":9") != std::string::npos, "frame recorder did not count native depth contents");
    const auto log = std::filesystem::path(directory) / "native-replay.log";
    auto command = "\"" + tool.string() + "\" \"" + directory + "\" --strict > \"" + log.string() + "\" 2>&1";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    const auto status = std::system(command.c_str());
    std::ifstream replay(log);
    const std::string text{std::istreambuf_iterator<char>(replay), std::istreambuf_iterator<char>()};
    std::fwrite(text.data(), 1u, text.size(), stderr);
    Require(status == 0, "native depth frame replay failed");
    Require(text.find("\"native_depth_surfaces\":9") != std::string::npos, "replay did not restore the native depth file");
    AgcDriver::DriverDetail::DriverTestAccess::Stop();
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--capture") Capture(argv[2]);
        else if (argc == 2 && std::string_view(argv[1]) == "--mirrorable_stencil") MirrorableStencil();
        else if (argc == 2 && std::string_view(argv[1]) == "--cross_aspect") CrossAspect();
        else if (argc == 2 && std::string_view(argv[1]) == "--preflight") Preflight();
        else { Require(argc == 1, "unknown native depth test arguments"); RoundTrip(); }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        if (argc == 3) AgcDriver::DriverDetail::DriverTestAccess::Stop();
        return 1;
    }
    std::puts("native depth capture GPU tests passed");
}
