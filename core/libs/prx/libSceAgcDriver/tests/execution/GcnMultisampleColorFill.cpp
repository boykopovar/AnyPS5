#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 64;
constexpr std::uint32_t Samples = 2;
constexpr std::uint32_t Format32 = 4u;
constexpr std::uint32_t ColorFormat32 = 4u;
constexpr std::uint32_t NumberFloat = 7u;
constexpr std::uint32_t FastClear = 1u << 13u;
constexpr std::uint32_t Thin2D = 14u;
constexpr std::uint32_t Display2D = 10u;
constexpr std::uint32_t Type2DMsaa = 14u;
constexpr std::uint32_t Xyzw = 0xfacu;
constexpr std::uint32_t Value = 0x3fc00000u;
constexpr std::size_t Slot = 0x80000;
constexpr std::size_t Slots = 18;
constexpr std::size_t CmaskSlot = Slots - 1u;
constexpr std::size_t CmaskBytes = 512u;
constexpr std::size_t BlockBytes = Slots * Slot;
constexpr std::uint64_t GcnAddressLimit = std::uint64_t{1} << 40u;
constexpr char NotUniform[] = "does not repeat one 32-bit value";
constexpr char OverTexture[] = "host target a texture built at";

class LowBlock {
public:
    LowBlock() {
#if defined(_WIN32) || defined(__APPLE__)
        data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
#ifdef _WIN32
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uint32_t ReadWriteProtection = 0x04;
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, ReadWriteProtection, BlockBytes);
#endif
#else
#ifdef ANYPS5_ENABLE_ASAN
        constexpr std::uintptr_t FirstHint = 0x60000000;
#else
        constexpr std::uintptr_t FirstHint = std::uintptr_t{1} << 32u;
#endif
        for (std::uintptr_t hint = FirstHint; data == nullptr && hint < GcnAddressLimit; hint += std::uintptr_t{1} << 32u) {
            void* mapped = mmap(reinterpret_cast<void*>(hint), BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            if (mapped != MAP_FAILED) data = static_cast<std::uint8_t*>(mapped);
        }
        if (data != nullptr && AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, BlockBytes);
#endif
        if (data == nullptr) return;
        std::memset(data, 0, BlockBytes);
        GuestAllocations::Mutation().Add(data, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(Address(), BlockBytes);
    }

    ~LowBlock() {
        if (data == nullptr) return;
        StorageTexture::FlushPending(Address(), BlockBytes, nullptr, "test release");
        GuestAllocations::Mutation().Remove(data);
#if defined(_WIN32) || defined(__APPLE__)
        GuestArena::GuestArenaReset_nid_postfix(data, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, BlockBytes);
#else
        munmap(data, BlockBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, BlockBytes);
#endif
    }

    LowBlock(const LowBlock&) = delete;
    LowBlock& operator=(const LowBlock&) = delete;

    bool Usable() const { return data != nullptr && Address() + BlockBytes <= GcnAddressLimit; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data)); }
    std::uint64_t At(std::size_t slot) const { return Address() + slot * Slot; }
    std::uint32_t* Words(std::size_t slot) const { return reinterpret_cast<std::uint32_t*>(data + slot * Slot); }

private:
    std::uint8_t* data = nullptr;
};

GuestTextureResource Resource(std::uint64_t address, std::uint32_t tileIndex, std::uint32_t width = Width, std::uint32_t height = Height, std::uint32_t pitch = Width) {
    const std::array<std::uint32_t, 8> gcn{
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32 << 20u) | (NumberFloat << 26u),
        (width - 1u) | ((height - 1u) << 14u),
        Xyzw | (1u << 16u) | (tileIndex << 20u) | (Type2DMsaa << 28u),
        (pitch - 1u) << 13u,
        0u,
        0u,
        0u,
    };
    const auto resource = DecodeTextureResource(ShaderRecompiler::GcnImageWords(std::span<const std::uint32_t, 8>(gcn)));
    Require(resource.baseAddress == address && resource.samples == Samples && resource.width == width && resource.height == height && resource.gcnPitch == pitch && resource.dimension == TextureDimension::k2DMsaa, "the PS4 T# did not decode as a 2-sample 2D MSAA texture of its size and pitch");
    Require(resource.tileMode == (tileIndex == Display2D ? TextureTileMode::kGcnDisplay2D : TextureTileMode::kGcnThin2D), "the PS4 T# did not decode in its 2D color tile mode");
    return resource;
}

std::uint64_t Footprint(const GuestTextureResource& resource) {
    const auto bytes = DescribeSurface(resource).guestBytes;
    Require(bytes >= std::uint64_t{resource.gcnPitch} * resource.height * 4u * Samples && bytes % Samples == 0u && bytes + 4096u <= Slot, "the PS4 2-sample texture's footprint does not fit its test slot");
    return bytes;
}

ColorTarget TargetOf(const GuestTextureResource& resource) {
    ColorTarget target{};
    target.tileMode = resource.tileMode == TextureTileMode::kGcnDisplay2D ? ColorTileMode::GcnDisplay2D : ColorTileMode::GcnThin2D;
    target.address = resource.baseAddress;
    target.surfaceAddress = resource.baseAddress;
    target.extent = {resource.width, resource.height};
    target.surfaceExtent = target.extent;
    target.format = VK_FORMAT_R32_SFLOAT;
    target.elementBytes = 4u;
    target.samples = Samples;
    target.pipeBankXor = resource.pipeBankXor;
    target.bytes = static_cast<std::size_t>(Footprint(resource) / Samples);
    return target;
}

ColorTarget DrawnTarget(std::uint64_t address, std::uint32_t flags = 0u, std::uint64_t cmask = 0u) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x319] = Width / 8u - 1u;
    cx[0x31a] = Width * Height / 64u - 1u;
    cx[0x31b] = 0u;
    cx[0x31c] = (ColorFormat32 << 2u) | (NumberFloat << 8u) | flags;
    cx[0x31d] = Thin2D | (1u << 12u) | (1u << 15u);
    cx[0x31f] = static_cast<std::uint32_t>(cmask >> 8u);
    cx[0x320] = static_cast<std::uint32_t>(CmaskBytes / 128u - 1u);
    const auto target = DecodeColorBuffer(cx, 0);
    Require(target.samples == Samples && target.tileMode == ColorTileMode::GcnThin2D && target.format == VK_FORMAT_R32_SFLOAT && target.bytes == std::size_t{Width} * Height * 4u && target.cmaskFastClear == ((flags & FastClear) != 0u), "the PS4 color target did not decode as a 2-sample 128x64 R32 float thin 2D target");
    return target;
}

template <typename Function>
std::string Failure(Function function) {
    try {
        function();
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

void CheckHeld(const Context& context, const ColorTarget& target, std::uint32_t expected, const std::string& what) {
    const auto bytes = ReadMultisampleTargetSamples_nid_no_patch(context, target);
    Require(bytes.size() == std::size_t{target.extent.width} * target.extent.height * Samples * 4u, "PS4 multisampled color fill: the samples read back from " + what + " are not 2 samples of its 32-bit texels");
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4u) {
        std::uint32_t word = 0;
        std::memcpy(&word, bytes.data() + offset, sizeof(word));
        if (word == expected) continue;
        char text[192];
        std::snprintf(text, sizeof(text), "PS4 multisampled color fill: %s holds 0x%08x at host sample byte %zu, not 0x%08x", what.c_str(), word, offset, expected);
        Require(false, text);
    }
}

void ExpectSamples(const Context& context, const GuestTextureResource& resource, std::uint32_t expected, const std::string& what, bool view = false) {
    const auto refusal = Failure([&] {
        const auto texture = view ? MultisampleTargetTexture(context, resource, VkComponentMapping{}) : MultisampleTargetFirstSample(context, resource, VkComponentMapping{});
        Require(texture != nullptr, "no texture was returned");
    });
    Require(refusal.empty(), "PS4 multisampled color fill: reading " + what + " was refused: " + refusal);
    CheckHeld(context, TargetOf(resource), expected, what);
}

void ExpectRefused(const Context& context, const GuestTextureResource& resource, const std::string& what) {
    const auto refusal = Failure([&] { static_cast<void>(MultisampleTargetFirstSample(context, resource, VkComponentMapping{})); });
    Require(!refusal.empty(), "PS4 multisampled color fill: reading " + what + " was accepted");
    Require(refusal.find(NotUniform) != std::string::npos, "PS4 multisampled color fill: reading " + what + " failed with another error: " + refusal);
}

void ExpectDrawStart(const Context& context, const ColorTarget& target, std::uint32_t expected, const std::string& what) {
    const auto refusal = Failure([&] { PrepareMultisampleTargetForDraw_nid_no_patch(context, target); });
    Require(refusal.empty(), "PS4 multisampled color fill: preparing the first draw into " + what + " was refused: " + refusal);
    CheckHeld(context, target, expected, what);
}

void ExpectDrawRefused(const Context& context, const ColorTarget& target, const std::string& expected, const std::string& what) {
    const auto refusal = Failure([&] { PrepareMultisampleTargetForDraw_nid_no_patch(context, target); });
    Require(refusal.find(expected) != std::string::npos, "PS4 multisampled color fill: preparing the first draw into " + what + " was not refused with \"" + expected + "\": " + refusal);
}

void Fill(const LowBlock& block, std::size_t slot, std::uint64_t bytes, std::uint32_t value) {
    std::fill(block.Words(slot), block.Words(slot) + bytes / 4u, value);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        ShaderRecompiler::SetGcnGuestShaders(true);
        const LowBlock block;
        if (!block.Usable()) {
            std::printf("skipped, no guest block below the 40-bit GCN address range (0x%llx)\n", static_cast<unsigned long long>(block.Address()));
            return VulkanTestSkipped;
        }
        auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        if ((context.limits.framebufferColorSampleCounts & Samples) == 0u) {
            std::printf("skipped, the device lacks 2-sample color targets (framebufferColorSampleCounts 0x%x)\n", static_cast<unsigned>(context.limits.framebufferColorSampleCounts));
            return VulkanTestSkipped;
        }
        TextureDetiler detiler(context);
        context.detiler = &detiler;
        Require(context.recorder != nullptr, "the test device's graphics context has no recorder");
        context.recorder->Activate();
        struct Cleanup {
            Recorder& recorder;
            VkDevice device;
            ~Cleanup() { recorder.Sync(); ClearMultisampleTargets(device); }
        } cleanup{*context.recorder, context.device};

        const auto bytes = Footprint(Resource(block.At(0), Thin2D));
        ExpectSamples(context, Resource(block.At(0), Thin2D), 0u, "zeroed color memory no draw rendered to");
        Fill(block, 1, bytes, Value);
        ExpectSamples(context, Resource(block.At(1), Thin2D), Value, "color memory holding one 32-bit value over its whole 2-sample footprint");
        Fill(block, 2, bytes, Value);
        block.Words(2)[1] = Value + 1u;
        ExpectRefused(context, Resource(block.At(2), Thin2D), "color memory with a second value in its second word");
        Fill(block, 3, bytes, Value);
        block.Words(3)[bytes / 4u - 1u] = Value + 1u;
        ExpectRefused(context, Resource(block.At(3), Thin2D), "color memory with a second value in the last word of its 2-sample footprint");
        Fill(block, 4, bytes, Value);
        block.Words(4)[bytes / 4u] = Value + 1u;
        ExpectSamples(context, Resource(block.At(4), Thin2D), Value, "color memory holding one value over its footprint and another right after it");
        const auto displayBytes = Footprint(Resource(block.At(5), Display2D));
        Fill(block, 5, displayBytes, Value);
        ExpectSamples(context, Resource(block.At(5), Display2D), Value, "display 2D color memory holding one 32-bit value");
        Fill(block, 6, bytes, Value);
        ExpectSamples(context, Resource(block.At(6), Thin2D), Value, "color memory holding one 32-bit value, through a multisampled view", true);

        const auto padded = Resource(block.At(7), Thin2D, 100u, 60u, 512u);
        const auto paddedBytes = Footprint(padded);
        Require(paddedBytes > std::uint64_t{512u} * 60u * 4u * Samples, "the 100x60 PS4 texture of pitch 512 is not padded past its 60 rows");
        Fill(block, 7, paddedBytes, Value);
        ExpectSamples(context, padded, Value, "a 100x60 texture of pitch 512 whose padded footprint holds one 32-bit value");
        Require(!MultisampledColorFillAbsorbs(padded.baseAddress, static_cast<std::size_t>(paddedBytes / Samples), Value), "PS4 multisampled color fill: a fill of half the padded footprint of the 100x60 texture of pitch 512 was absorbed as a fill of all of it");
        Require(MultisampledColorFillAbsorbs(padded.baseAddress, static_cast<std::size_t>(paddedBytes), Value), "PS4 multisampled color fill: a fill of one value over the whole padded footprint of the 100x60 texture of pitch 512 was not absorbed by its host target");
        Fill(block, 8, paddedBytes, Value);
        block.Words(8)[paddedBytes / 4u - 1u] = Value + 1u;
        ExpectRefused(context, Resource(block.At(8), Thin2D, 100u, 60u, 512u), "a 100x60 texture of pitch 512 with a second value in the last word of its padded footprint");
        Fill(block, 9, paddedBytes, Value);
        block.Words(9)[paddedBytes / 4u] = Value + 1u;
        ExpectSamples(context, Resource(block.At(9), Thin2D, 100u, 60u, 512u), Value, "a 100x60 texture of pitch 512 with another value right after its padded footprint");

        ExpectDrawStart(context, DrawnTarget(block.At(10)), 0u, "the zeroed color memory of a 2-sample PS4 target");
        const auto drawnValue = DrawnTarget(block.At(11));
        Fill(block, 11, Samples * drawnValue.bytes, Value);
        ExpectDrawStart(context, drawnValue, Value, "a 2-sample PS4 target whose whole color memory repeats one value");
        const auto drawnMixed = DrawnTarget(block.At(12));
        Fill(block, 12, Samples * drawnMixed.bytes, Value);
        block.Words(12)[drawnMixed.bytes / 4u] = Value + 1u;
        ExpectDrawRefused(context, drawnMixed, "nonuniform initial multisample color tiling is not implemented", "a 2-sample PS4 target with a second value halfway through its color memory");

        ExpectSamples(context, Resource(block.At(13), Thin2D), 0u, "zeroed color memory sampled before a draw of the same size");
        ExpectDrawStart(context, DrawnTarget(block.At(13)), 0u, "a 2-sample PS4 target a 128x64 texture of the same memory was sampled through");
        ExpectSamples(context, Resource(block.At(14), Thin2D, 120u, 60u, 128u), 0u, "zeroed color memory sampled through a 120x60 texture");
        ExpectDrawRefused(context, DrawnTarget(block.At(14), FastClear, block.At(CmaskSlot)), OverTexture, "a fast-cleared 128x64 target over memory a 120x60 texture was sampled through");
        ExpectSamples(context, Resource(block.At(15), Thin2D, 120u, 60u, 128u), 0u, "zeroed color memory sampled through another 120x60 texture");
        ExpectDrawRefused(context, DrawnTarget(block.At(15)), OverTexture, "a 128x64 target over memory a 120x60 texture was sampled through");
        ExpectSamples(context, Resource(block.At(16), Thin2D, 120u, 60u, 128u), 0u, "zeroed color memory sampled through a third 120x60 texture");
        const auto materialized = Failure([&] { MaterializeMultisampleTargetClear(context, DrawnTarget(block.At(16), FastClear, block.At(CmaskSlot))); });
        Require(materialized.find(OverTexture) != std::string::npos, "PS4 multisampled color fill: materializing the fast clear of a 128x64 target over memory a 120x60 texture was sampled through, with no draw, was not refused: " + materialized);

        std::printf("PS4 multisampled color fill tests passed: 2-sample 32-bit float 2D-tiled textures no draw rendered to (128x64, and 100x60 of pitch 512 whose %llu-byte footprint is padded) hold, in every sample, the value their footprint repeats, and a second value inside it is refused; a 2-sample PS4 target's first draw starts from the value its color memory repeats, reuses a texture's host target of its size and is refused over one of another size\n", static_cast<unsigned long long>(paddedBytes));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
