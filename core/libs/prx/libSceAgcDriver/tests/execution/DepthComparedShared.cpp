#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using DepthFastClearHarness::Require;
using namespace AgcDriver::Graphics;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;
constexpr VkExtent2D Extent{Width, Height};
constexpr std::size_t Block = 65536;
constexpr std::size_t Bytes = 2 * Block;
constexpr std::size_t Page = 4096;
constexpr std::size_t UnitTexels = Block / sizeof(float);

class DirectMemory {
public:
    DirectMemory() {
        memory = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(2 * Bytes, Block));
        Require(memory != nullptr, "cannot reserve compared depth memory");
        section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, Bytes, nullptr);
        if (section == nullptr) {
            GuestArena::GuestArenaRelease_nid_postfix(memory, 2 * Bytes);
            memory = nullptr;
            Require(false, "cannot create compared depth backing");
        }
        GuestArena::GuestArenaForgetSection_nid_postfix(section);
        GuestArena::GuestArenaMapDirect_nid_postfix(memory, Bytes, section, 0, PAGE_READWRITE);
        GuestAllocations::Mutation mutation;
        mutation.Add(memory, Bytes, true, true, true);
        mutation.NoteGpuMapping(memory, Bytes);
    }

    ~DirectMemory() {
        if (memory != nullptr) {
            {
                GuestAllocations::Mutation mutation;
                if (shared) mutation.Remove(memory + Bytes);
                mutation.Remove(memory);
            }
            GuestArena::GuestArenaReset_nid_postfix(memory, 2 * Bytes);
            GuestArena::GuestArenaRelease_nid_postfix(memory, 2 * Bytes);
        }
        if (section != nullptr) {
            GuestArena::GuestArenaForgetSection_nid_postfix(section);
            CloseHandle(section);
        }
    }

    DirectMemory(const DirectMemory&) = delete;
    DirectMemory& operator=(const DirectMemory&) = delete;

    std::uint64_t Address() const { return reinterpret_cast<std::uint64_t>(memory); }

    void Share() {
        GuestAllocations::Mutation mutation;
        std::uintptr_t start = 0;
        std::uintptr_t end = 0;
        if (GuestArena::GuestArenaResidentSpan_nid_postfix(Address(), Bytes, &start, &end)) mutation.RequireUnpinned(reinterpret_cast<const void*>(start), end - start);
        GuestArena::GuestArenaMapDirect_nid_postfix(memory + Bytes, Bytes, section, 0, PAGE_READWRITE);
        mutation.Add(memory + Bytes, Bytes, true, true, true);
        mutation.NoteGpuMapping(memory + Bytes, Bytes);
        shared = true;
    }

    void FillUnit(std::size_t unit, float value, bool alias = false) {
        auto* values = reinterpret_cast<float*>(memory + unit * Block + (alias && shared ? Bytes : 0));
        std::fill_n(values, UnitTexels, value);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        RequireUnit(unit, value, "the original depth mapping did not see the CPU store");
    }

    void RequireUnit(std::size_t unit, float value, const std::string& what) const {
        const auto* original = reinterpret_cast<const float*>(memory + unit * Block);
        Require(std::all_of(original, original + UnitTexels, [&](float held) { return held == value; }), what);
    }

    void CheckTracking(const std::string& what) const {
        MEMORY_BASIC_INFORMATION info{};
        Require(VirtualQuery(memory, &info, sizeof(info)) == sizeof(info), what + ": cannot query the depth mapping");
        const auto collected = AgcDriver::GuestMemory::CollectWritesUncached(Address(), Bytes);
        if (!shared) {
            Require(info.Type == MEM_PRIVATE && AgcDriver::GuestMemory::Watched(Address(), Bytes) && collected != 0, what + ": the private control is not tracked resident memory");
            return;
        }
        std::array<void*, Bytes / Page> pages{};
        auto count = pages.size();
        GuestArena::GuestArenaComparedPages_nid_no_patch(Address(), Bytes, pages.data(), &count);
        Require(info.Type == MEM_MAPPED && info.Protect == PAGE_READWRITE && collected == 0 && count == pages.size(), what + ": the shared depth plane is not compared memory");
    }

private:
    std::byte* memory = nullptr;
    HANDLE section = nullptr;
    bool shared = false;
};

void ClearDepthImage(const Context& context, VkImageView view, float value) {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    const auto destroy = [&] {
        if (framebuffer != VK_NULL_HANDLE) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
        if (renderPass != VK_NULL_HANDLE) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, renderPass, nullptr);
    };
    try {
        VkAttachmentDescription attachment{};
        attachment.format = VK_FORMAT_D32_SFLOAT;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
        const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_GENERAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = &reference;
        constexpr auto tests = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        const std::array<VkSubpassDependency, 2> dependencies{{
            {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, tests, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0},
            {0, VK_SUBPASS_EXTERNAL, tests, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, 0},
        }};
        VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        passInfo.attachmentCount = 1;
        passInfo.pAttachments = &attachment;
        passInfo.subpassCount = 1;
        passInfo.pSubpasses = &subpass;
        passInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
        passInfo.pDependencies = dependencies.data();
        Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &renderPass), "vkCreateRenderPass");
        VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &view;
        framebufferInfo.width = Width;
        framebufferInfo.height = Height;
        framebufferInfo.layers = 1;
        Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebufferInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
        CommandBatch batch(context);
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = renderPass;
        begin.framebuffer = framebuffer;
        begin.renderArea.extent = Extent;
        context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(batch.Handle(), &begin, VK_SUBPASS_CONTENTS_INLINE);
        const VkClearAttachment clear{VK_IMAGE_ASPECT_DEPTH_BIT, 0, {.depthStencil = {value, 0}}};
        const VkClearRect rectangle{{{0, 0}, Extent}, 0, 1};
        context.Function<PFN_vkCmdClearAttachments>("vkCmdClearAttachments")(batch.Handle(), 1, &clear, 1, &rectangle);
        context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(batch.Handle());
        batch.SubmitAndWait();
    } catch (...) {
        destroy();
        throw;
    }
    destroy();
}

void Run(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& shaders, DirectMemory& memory, bool share) {
    const std::string kind = share ? "late-shared compared depth" : "resident private depth";
    alignas(256) std::array<std::byte, static_cast<std::size_t>(Width) * Height * 4> pixels{};
    const auto swizzle = XorSwizzleMode(TextureTileMode::kZ64KBX);
    const DepthFastClearHarness::Surface depth{memory.Address(), 0, 0, false, VK_FORMAT_D32_SFLOAT, 0, 0, swizzle};
    const auto draw = [&](const DepthFastClearHarness::DrawOptions& options) {
        return DepthFastClearHarness::Draw(device, shaders, depth, options, Extent, pixels);
    };
    const auto expect = [&](std::array<bool, 2> covered, const std::string& phase) {
        const auto count = draw({});
        const auto expected = static_cast<std::uint32_t>(UnitTexels * (static_cast<unsigned>(covered[0]) + static_cast<unsigned>(covered[1])));
        Require(count == expected, kind + ": " + phase + " covered " + std::to_string(count) + " pixels, expected " + std::to_string(expected));
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const auto at = (static_cast<std::size_t>(y) * Width + x) * 4;
                const bool seen = pixels[at] == std::byte{255};
                if (seen != covered[x / 128]) Require(false, kind + ": " + phase + " has wrong coverage at " + std::to_string(x) + "," + std::to_string(y));
            }
        }
        std::printf("%s: %s covered %u pixels\n", kind.c_str(), phase.c_str(), count);
    };
    memory.FillUnit(0, 1.0f);
    memory.FillUnit(1, 1.0f);
    memory.CheckTracking(kind + " before its first draw");
    Require(AgcDriver::GuestMemory::ImportWatched(memory.Address(), Bytes, [] { return true; }), "cannot mark the resident depth memory for comparison when shared");
    Require(draw({.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Width * Height, kind + ": its first depth draw did not cover the target");
    FlushDepthPlanes(memory.Address(), Bytes);
    device.WaitIdle();
    if (share) memory.Share();
    memory.CheckTracking(kind + " after optional sharing");
    memory.FillUnit(0, 0.75f, share);
    expect({true, false}, share ? "the first alias write after sharing reloads the left unit" : "the first CPU write reloads the left unit");
    draw({.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS});
    memory.FillUnit(0, 1.0f);
    memory.FillUnit(1, 1.0f);
    Require(draw({.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Width * Height, kind + ": its pending depth baseline did not cover the target");
    AgcDriver::GuestMemory::BumpCollectEpoch();
    expect({false, false}, "an unchanged bind preserves pending depth 0.5");
    AgcDriver::GuestMemory::BumpCollectEpoch();
    expect({false, false}, "another unchanged epoch preserves pending depth 0.5");
    if (!share) {
        memory.FillUnit(0, 1.0f);
        expect({true, false}, "a same-value CPU store reloads the left unit");
        Require(draw({.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Width * Height, kind + ": redrawing depth after a same-value CPU store did not cover the target");
        AgcDriver::GuestMemory::BumpCollectEpoch();
        expect({false, false}, "an unchanged bind after a same-value CPU store preserves newer GPU results");
    }
    memory.FillUnit(0, 0.75f);
    expect({true, false}, "a CPU write reloads only the left unit");
    AgcDriver::GuestMemory::BumpCollectEpoch();
    expect({true, false}, "an unchanged bind keeps the right unit pending");
    memory.FillUnit(0, 0.25f, share);
    expect({false, false}, share ? "a second CPU write through the alias reloads the left unit" : "a second CPU write reloads the left unit");
    memory.FillUnit(1, 0.75f, share);
    expect({false, true}, share ? "a CPU write through the alias reloads the right unit" : "a CPU write reloads the right unit");
    Require(draw({.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Width * Height, kind + ": redrawing pending depth did not cover the target");
    AgcDriver::GuestMemory::BumpCollectEpoch();
    expect({false, false}, "an unchanged bind preserves newer GPU results");
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    DepthTarget target{memory.Address(), 0, Extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0};
    target.depthSwizzle = swizzle;
    const auto view = DepthSurfaceView(context, target, false);
    const auto stored = FlushDepthPlanes(memory.Address(), Block);
    device.WaitIdle();
    Require(stored.size() == 1 && stored.front().first == memory.Address() && stored.front().second == Block, kind + ": a partial flush did not store the left unit alone");
    memory.RequireUnit(0, 0.5f, kind + ": the left unit's GPU depth did not reach guest memory");
    memory.RequireUnit(1, 0.75f, kind + ": a partial flush wrote the right unit that should stay pending");
    ClearDepthImage(context, view, 0.75f);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    expect({true, true}, "an unchanged bind after write-back preserves a newer GPU clear");
    Require(draw({.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Width * Height, kind + ": restoring pending depth after the clear did not cover the target");
    memory.FillUnit(0, 0.75f, share);
    expect({true, false}, share ? "an alias write after a partial flush reloads only the left unit" : "a CPU write after a partial flush reloads only the left unit");
    memory.CheckTracking(kind + " after its CPU updates");
}

void OwnerSwitch(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& shaders, DirectMemory& memory) {
    alignas(256) static std::array<std::byte, Block> stencilA{};
    alignas(256) static std::array<std::byte, Block> stencilB{};
    alignas(256) static std::array<std::uint32_t, 32768 / sizeof(std::uint32_t)> htile{};
    alignas(256) std::array<std::byte, static_cast<std::size_t>(Width) * Height * 4> pixels{};
    Require(HtileMetadataBytes(Extent) == sizeof(htile), "the owner-switch HTILE does not cover the depth plane");
    std::fill(htile.begin(), htile.end(), 0xffffffffu);
    const auto swizzle = XorSwizzleMode(TextureTileMode::kZ64KBX);
    const DepthFastClearHarness::Surface a{memory.Address(), reinterpret_cast<std::uint64_t>(stencilA.data()), 0, false, VK_FORMAT_D32_SFLOAT_S8_UINT, 0, 0, swizzle};
    const DepthFastClearHarness::Surface b{memory.Address(), reinterpret_cast<std::uint64_t>(stencilB.data()), reinterpret_cast<std::uint64_t>(htile.data()), false, VK_FORMAT_D32_SFLOAT_S8_UINT, 0, 0, swizzle};
    const auto expect = [&](const DepthFastClearHarness::Surface& surface, std::array<bool, 2> covered, const std::string& phase) {
        const auto count = DepthFastClearHarness::Draw(device, shaders, surface, {}, Extent, pixels);
        const auto expected = static_cast<std::uint32_t>(UnitTexels * (static_cast<unsigned>(covered[0]) + static_cast<unsigned>(covered[1])));
        Require(count == expected, phase + " covered " + std::to_string(count) + " pixels, expected " + std::to_string(expected));
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const bool seen = pixels[(static_cast<std::size_t>(y) * Width + x) * 4] == std::byte{255};
                if (seen != covered[x / 128]) Require(false, phase + " has wrong coverage at " + std::to_string(x) + "," + std::to_string(y));
            }
        }
        std::printf("owner switch: %s covered %u pixels\n", phase.c_str(), count);
    };
    memory.FillUnit(0, 1.0f);
    memory.FillUnit(1, 1.0f);
    Require(AgcDriver::GuestMemory::ImportWatched(memory.Address(), Bytes, [] { return true; }), "cannot mark the owner-switch depth memory for comparison when shared");
    Require(DepthFastClearHarness::Draw(device, shaders, a, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, Extent, pixels) == Width * Height, "owner A did not establish GPU depth 0.5");
    FlushDepthPlanes(memory.Address(), Bytes);
    device.WaitIdle();
    memory.Share();
    memory.CheckTracking("the owner-switch shared depth plane");
    memory.FillUnit(0, 0.25f, true);
    memory.RequireUnit(1, 0.5f, "sharing changed the owner-switch right unit before its clear");
    Require(!NoteHtileDepthClear(b.htile, sizeof(htile)), "a future single-sample owner requested native clear completion");
    expect(b, {true, true}, "a fresh owner's HTILE clear supersedes a prior CPU write");
    expect(a, {true, true}, "returning to owner A inherits the explicit depth clear");
    AgcDriver::GuestMemory::BumpCollectEpoch();
    expect(a, {true, true}, "an unchanged epoch preserves the inherited depth clear");
    memory.FillUnit(1, 0.25f, true);
    expect(a, {true, false}, "an alias CPU write after owner inheritance reloads only the right unit");
}

void CompletionUnmap(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& shaders, DirectMemory& memory) {
    memory.FillUnit(0, 1.0f);
    memory.FillUnit(1, 1.0f);
    Require(AgcDriver::GuestMemory::ImportWatched(memory.Address(), Bytes, [] { return true; }), "cannot mark completion-unmap depth memory for comparison when shared");
    memory.Share();
    memory.CheckTracking("completion-unmap shared depth");
    alignas(256) std::array<std::byte, static_cast<std::size_t>(Width) * Height * 4> pixels{};
    const DepthFastClearHarness::Surface depth{memory.Address(), 0, 0, false, VK_FORMAT_D32_SFLOAT, 0, 0, XorSwizzleMode(TextureTileMode::kZ64KBX)};
    Require(DepthFastClearHarness::Draw(device, shaders, depth, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, Extent, pixels) == Width * Height, "the completion-unmap draw did not cover the depth plane");
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr && recorder->Device() == context.device, "completion-unmap has no active recorder for its device");
    bool completed = false;
    recorder->OnComplete([&] {
        Require(Recorder::InCompletion(), "the unmap notification did not run inside a completion");
        RetireUnmappedDepth_nid_no_patch(memory.Address() + Block, Block);
        Require(FlushDepthPlanes(memory.Address() + Block, Block).empty(), "a completion stored the depth unit whose guest memory was unmapped");
        completed = true;
    });
    const auto stored = FlushDepthPlanes(memory.Address(), Block);
    device.WaitIdle();
    Require(completed, "the completion-unmap callback did not run");
    Require(stored.size() == 1 && stored.front().first == memory.Address() && stored.front().second == Block, "completion-unmap did not store the left unit alone");
    memory.RequireUnit(0, 0.5f, "completion-unmap lost the left unit's completed GPU depth");
    Require(FlushDepthPlanes(memory.Address() + Block, Block).empty(), "completion-deferred depth forgot the right unit's unmap notification");
    device.WaitIdle();
    memory.RequireUnit(1, 1.0f, "completion-deferred GPU depth overwrote the unmapped unit's unchanged guest bytes");
    std::puts("completion-deferred depth retains its unmap notification");
}

}
#endif

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
#ifndef _WIN32
    std::puts("skipped, compared shared depth uses Windows direct mappings");
    return VulkanTestSkipped;
#else
    try {
        if (!AgcDriver::GuestMemory::WriteWatched()) {
            std::puts("skipped, guest memory has no write watch");
            return VulkanTestSkipped;
        }
        Require(DepthSliceBytes(Extent, sizeof(float)) == Bytes, "the depth plane is not two 128x128 units");
        DirectMemory resident;
        DirectMemory shared;
        DirectMemory owners;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        PrepareImportWatch(context);
        SetImportWatch(context, ImportWatch::Watch);
        const auto shaders = DepthFastClearHarness::Compile(*device);
        if (argc == 2 && std::string_view(argv[1]) == "--completion-unmap") {
            CompletionUnmap(*device, shaders, shared);
            return 0;
        }
        Run(*device, shaders, resident, false);
        Run(*device, shaders, shared, true);
        OwnerSwitch(*device, shaders, owners);
        std::puts("compared shared depth CPU updates preserve pending units");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
#endif
}
