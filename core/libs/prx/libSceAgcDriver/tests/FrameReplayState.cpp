#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/NativeDepthCapture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "execution/DepthFastClearHarness.hpp"
#include "NarrowConstantStoreFixture.hpp"
#define main frameReplayToolMain
#include "../tools/FrameReplay.cpp"
#undef main
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <limits>
#include <string_view>
#include <barrier>
#include <bit>
#include <cmath>
#include <new>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static void UseDevice(std::shared_ptr<VulkanDevice> value) { Driver::Get().device = std::move(value); }
    static std::shared_ptr<VulkanDevice> Device() { return Driver::Get().device.Load(); }
};

}

namespace {

using AgcDriver::FrameCaptureWriter;

constexpr std::uint32_t ReadWrite = 0x04;

void check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint64_t address(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

std::byte* allocate(std::size_t size, bool gpu = true) {
    auto* bytes = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(size, 65536));
    check(bytes != nullptr, "guest allocation failed");
    GuestArena::GuestArenaCommit_nid_postfix(bytes, size, 0x04, size);
#ifdef __linux__
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(bytes, size);
#endif
    GuestAllocations::Mutation mutation;
    mutation.Add(bytes, size, true, true, gpu);
    return bytes;
}

void collectGroups(Replay& replay) {
    for (const auto& chunk : replay.reader.Chunks()) {
        if (chunk.kind == FrameMemoryKind::Delta) replay.groups[chunk.group].push_back(&chunk);
    }
}

FrameCall memoryCall(std::uint64_t group, std::uint64_t thread) {
    FrameCall call;
    call.kind = FrameCallKind::Memory;
    call.values = {group};
    call.thread = thread;
    return call;
}

void checkNativeRepeatContents(std::span<const AgcDriver::NativeDepthSnapshot> expected, std::span<const AgcDriver::NativeDepthSnapshot> actual) {
    check(actual.size() == expected.size(), "native repeated reset lost a current plane or retained a window-only companion");
    std::vector<std::pair<std::uint64_t, std::uint64_t>> orders;
    for (const auto& before : expected) {
        const auto found = std::ranges::find_if(actual, [&](const auto& after) { return after.target.address == before.target.address && after.target.stencilAddress == before.target.stencilAddress && after.target.format == before.target.format; });
        check(found != actual.end() && found->depth == before.depth && found->stencil == before.stencil, "native repeated reset did not restore every captured depth and stencil texel before a draw");
        if (before.depthOrder != 0u) orders.emplace_back(before.depthOrder, found->depthOrder);
        if (before.stencilOrder != 0u) orders.emplace_back(before.stencilOrder, found->stencilOrder);
    }
    for (const auto& left : orders) {
        check(left.second != 0u, "native repeated reset omitted a captured plane binding");
        for (const auto& right : orders) check((left.first < right.first) == (left.second < right.second), "native repeated reset changed relative cross-aspect binding precedence");
    }
}

void testNativeRepeat(const std::filesystem::path& directory, std::string_view mode) {
    constexpr std::size_t Step = 131072u;
    constexpr std::size_t Bytes = 16u * Step;
    constexpr VkExtent2D Extent{DepthFastClearHarness::Width, DepthFastClearHarness::Height};
    const auto texels = Extent.width * Extent.height;
    auto* bytes = allocate(Bytes);
    const auto begin = address(bytes);
    std::uintptr_t arena = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arena, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(begin, Bytes, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.Finish("{\"arena_base\":" + std::to_string(arena) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0u, "native repeated reset skipped captured guest memory");
    auto since = watchBaseline(reader, skipped);
    auto opened = OpenVulkanTestDevice();
    check(opened != nullptr, "native repeated reset device unavailable");
    auto device = std::shared_ptr<AgcDriver::VulkanDevice>(std::move(opened));
    AgcDriver::DriverDetail::DriverTestAccess::UseDevice(device);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    const auto shaders = DepthFastClearHarness::Compile(*device);
    const bool companions = mode == "native_repeat_companions";
    const bool clean = mode == "native_repeat_clean";
    const auto surface = [&](unsigned index) {
        return DepthFastClearHarness::Surface{begin + index * Step, index == 1u || index == 2u ? begin + 12u * Step : 0u, 0u, false, index == 1u || index == 2u ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_D16_UNORM};
    };
    const auto draw = [&](bool mutated) {
        const auto count = clean ? 1u : 3u;
        for (unsigned index = 0; index < count; ++index) {
            DepthFastClearHarness::DrawOptions options;
            options.depthWrite = true;
            options.depthCompare = VK_COMPARE_OP_ALWAYS;
            options.minDepth = options.maxDepth = mutated ? 0.875f : static_cast<float>(index + 1u) * 0.25f;
            options.stencilTest = index != 0u;
            options.stencil = {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 255u, 255u, mutated ? 91u : index == 1u ? 37u : 203u};
            check(DepthFastClearHarness::Draw(*device, shaders, surface(index), options) == texels, "native repeated reset mutation did not cover its attachment");
        }
    };
    draw(false);
    if (companions) {
        const AgcDriver::Graphics::DepthTarget companion{begin + 3u * Step, 0u, Extent, VK_FORMAT_D16_UNORM, 0.625f, 0u};
        static_cast<void>(AgcDriver::Graphics::DepthSurfaceView(context, companion, false, false));
    }
    device->WaitIdle();
    const auto captured = device->CaptureNativeDepth();
    check(captured.size() == (clean ? 1u : companions ? 4u : 3u), "native repeated reset fixture did not capture its depth/stencil graph");
    for (const auto& snapshot : captured) {
        const auto index = static_cast<unsigned>((snapshot.target.address - begin) / Step);
        const bool d16 = index == 0u || index == 3u;
        const auto expected = index == 3u ? 0.625f : static_cast<float>(index + 1u) * 0.25f;
        check(snapshot.depthOrder != 0u && snapshot.depth.size() == texels * (d16 ? 2u : 4u), "native repeated reset fixture has an incomplete captured depth plane");
        for (std::size_t pixel = 0u; pixel < texels; ++pixel) {
            if (d16) {
                std::uint16_t bits = 0u;
                std::memcpy(&bits, snapshot.depth.data() + pixel * 2u, sizeof(bits));
                check(std::abs(static_cast<float>(bits) / 65535.0f - expected) < 2.0f / 65535.0f, "native repeated reset fixture captured the wrong D16 contents");
            } else {
                std::uint32_t bits = 0u;
                std::memcpy(&bits, snapshot.depth.data() + pixel * 4u, sizeof(bits));
                check(bits == std::bit_cast<std::uint32_t>(expected), "native repeated reset fixture captured the wrong D32 contents");
            }
        }
        if (index == 2u) check(snapshot.stencil.size() == texels && std::ranges::all_of(snapshot.stencil, [](std::byte value) { return value == std::byte{203}; }), "native repeated reset fixture lost the captured shared stencil owner");
        else check(snapshot.stencil.empty(), "native repeated reset fixture captured a nonowner stencil plane");
    }
    const auto d16 = surface(0u);
    const auto holdWords = std::array<std::uint32_t, 8>{static_cast<std::uint32_t>(d16.depth >> 8u), static_cast<std::uint32_t>((d16.depth >> 40u) & 255u) | (7u << 20u) | (((Extent.width - 1u) & 3u) << 30u), ((Extent.width - 1u) >> 2u) | ((Extent.height - 1u) << 14u), 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
    const auto holdResource = AgcDriver::Graphics::DecodeTextureResource(holdWords);
    const AgcDriver::Graphics::DepthTarget windowOnly{begin + 4u * Step, 0u, Extent, VK_FORMAT_D16_UNORM, 0.875f, 0u};
    static_cast<void>(AgcDriver::Graphics::DepthSurfaceView(context, windowOnly, false, false));
    auto held = AgcDriver::Graphics::DepthSurfaceTexture(context, holdWords, holdResource, {});
    check(held != nullptr, "native repeated reset fixture could not retain the old sampled view");
    for (unsigned pass = 1u; pass < 3u; ++pass) {
        draw(true);
        static_cast<void>(AgcDriver::Graphics::DepthSurfaceView(context, windowOnly, false, false));
        device->WaitIdle();
        Runs changed;
        static_cast<void>(changedSince(begin, Step, since, changed));
        check(changed.empty(), "native GPU-only D16 mutation unexpectedly changed captured guest pages");
        const auto reset = resetMemory(reader, skipped, since, ResetFault::None, true, &captured);
        check(reset.retired.depthSurfaces >= (clean ? 2u : companions ? 5u : 4u), "native repeated reset did not discard the full preceding depth graph");
        check(!AgcDriver::Graphics::DepthSurfaceHolds(context, holdResource, held.get()), "native repeated reset retained an unmirrored GPU-only D16 owner from the previous window");
        check(!AgcDriver::Graphics::ActiveDepthExtent(device->Device(), begin + 4u * Step).has_value(), "native repeated reset retained a clean window-only native companion");
        check(AgcDriver::DriverDetail::DriverTestAccess::Device().get() == device.get(), "native repeated reset replaced the VulkanDevice object");
        const auto restored = device->CaptureNativeDepth();
        checkNativeRepeatContents(captured, restored);
        check(!AgcDriver::Graphics::DepthSurfaceHolds(context, holdResource, held.get()), "native repeated reset made the retained old view a current owner");
        std::printf("native repeat pass %u restored %zu exact depth/stencil records on the same device\n", pass + 1u, restored.size());
    }
    held.reset();
    draw(false);
    device->WaitIdle();
}

void testNativeRetiredStoredOwner(const std::filesystem::path& directory) {
    constexpr std::size_t Step = 131072u;
    constexpr std::size_t Bytes = 16u * Step;
    constexpr VkExtent2D Extent{DepthFastClearHarness::Width, DepthFastClearHarness::Height};
    const auto texels = Extent.width * Extent.height;
    auto* bytes = allocate(Bytes);
    const auto begin = address(bytes);
    std::uintptr_t arena = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arena, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(begin, Bytes, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.Finish("{\"arena_base\":" + std::to_string(arena) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0u, "retired native owner fixture skipped captured guest memory");
    auto since = watchBaseline(reader, skipped);
    auto opened = OpenVulkanTestDevice();
    check(opened != nullptr, "retired native owner device unavailable");
    auto device = std::shared_ptr<AgcDriver::VulkanDevice>(std::move(opened));
    AgcDriver::DriverDetail::DriverTestAccess::UseDevice(device);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    const auto shaders = DepthFastClearHarness::Compile(*device);
    const DepthFastClearHarness::Surface surface{begin, 0u, 0u, false, VK_FORMAT_D16_UNORM};
    DepthFastClearHarness::DrawOptions options;
    options.depthWrite = true;
    options.depthCompare = VK_COMPARE_OP_ALWAYS;
    options.minDepth = options.maxDepth = 0.25f;
    check(DepthFastClearHarness::Draw(*device, shaders, surface, options) == texels, "retired native owner initial draw failed");
    const auto captured = device->CaptureNativeDepth();
    check(captured.size() == 1u && captured.front().depth.size() == texels * 2u, "retired native owner fixture did not capture one complete D16 plane");
    for (std::size_t pixel = 0u; pixel < texels; ++pixel) {
        std::uint16_t bits = 0u;
        std::memcpy(&bits, captured.front().depth.data() + pixel * 2u, sizeof(bits));
        check(std::abs(static_cast<float>(bits) / 65535.0f - 0.25f) < 2.0f / 65535.0f, "retired native owner fixture captured the wrong initial texel");
    }
    const AgcDriver::Graphics::DepthTarget other{begin + 2u * Step, 0u, Extent, VK_FORMAT_D16_UNORM, 1.0f, 0u};
    static_cast<void>(AgcDriver::Graphics::DepthSurfaceView(context, other, false, false));
    const std::array<std::uint32_t, 8> words{static_cast<std::uint32_t>(begin >> 8u), static_cast<std::uint32_t>((begin >> 40u) & 255u) | (7u << 20u) | (((Extent.width - 1u) & 3u) << 30u), ((Extent.width - 1u) >> 2u) | ((Extent.height - 1u) << 14u), 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    auto held = AgcDriver::Graphics::DepthSurfaceTexture(context, words, resource, {});
    check(held != nullptr && held.use_count() > 1u, "retired native owner fixture did not retain the native sampled texture");
    const auto oldSampledView = held->View();
    const auto oldAttachmentView = AgcDriver::Graphics::DepthSurfaceOrderTarget(context, captured.front().target).view;
    options.minDepth = options.maxDepth = 0.75f;
    check(DepthFastClearHarness::Draw(*device, shaders, surface, options) == texels, "retired native owner mutation draw failed");
    static_cast<void>(AgcDriver::Graphics::DepthSurfaceView(context, other, false, false));
    device->WaitIdle();
    AgcDriver::Graphics::RetireDepthSurfaces(device->Device(), begin, Step);
    check(!AgcDriver::Graphics::ActiveDepthExtent(device->Device(), begin).has_value(), "ordinary native retirement did not remove the current owner");
    check(AgcDriver::Graphics::DepthSurfaceOrderTarget(context, captured.front().target).view == oldAttachmentView, "ordinary retirement did not leave the retained native attachment active for this control");
    check(held->View() == oldSampledView && held.use_count() > 1u, "ordinary retirement released the retained sampled texture before the reset");
    std::puts("native stored-owner control: retired attachment remains active and sampled texture retained before reset");
    static_cast<void>(resetMemory(reader, skipped, since, ResetFault::None, true, &captured));
    check(!AgcDriver::Graphics::DepthSurfaceHolds(context, resource, held.get()), "native repeated reset reactivated a retired-but-active held depth image");
    check(AgcDriver::Graphics::DepthSurfaceOrderTarget(context, captured.front().target).view != oldAttachmentView, "native repeated reset reused the retired attachment view");
    check(AgcDriver::DriverDetail::DriverTestAccess::Device().get() == device.get(), "retired native owner reset replaced the VulkanDevice object");
    checkNativeRepeatContents(captured, device->CaptureNativeDepth());
    held.reset();
    static_cast<void>(AgcDriver::Graphics::DepthSurfaceView(context, other, false, false));
    options.depthWrite = false;
    options.depthCompare = VK_COMPARE_OP_EQUAL;
    options.minDepth = options.maxDepth = 0.25f;
    check(DepthFastClearHarness::Draw(*device, shaders, surface, options) == texels, "retired native owner failed to bind after the old hold was released");
    device->WaitIdle();
}

void testNativeRepeatPreflight(const std::filesystem::path& directory) {
    constexpr std::size_t Step = 131072u;
    constexpr std::size_t Bytes = 16u * Step;
    constexpr VkExtent2D Extent{DepthFastClearHarness::Width, DepthFastClearHarness::Height};
    const auto texels = Extent.width * Extent.height;
    auto* bytes = allocate(Bytes);
    const auto begin = address(bytes);
    std::uintptr_t arena = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arena, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(begin, Bytes, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.Finish("{\"arena_base\":" + std::to_string(arena) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0u, "native preflight preservation skipped captured guest memory");
    auto since = watchBaseline(reader, skipped);
    auto opened = OpenVulkanTestDevice();
    check(opened != nullptr, "native preflight preservation device unavailable");
    auto device = std::shared_ptr<AgcDriver::VulkanDevice>(std::move(opened));
    AgcDriver::DriverDetail::DriverTestAccess::UseDevice(device);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    const auto shaders = DepthFastClearHarness::Compile(*device);
    const auto draw = [&](bool mutated) {
        for (unsigned index = 0u; index < 2u; ++index) {
            DepthFastClearHarness::DrawOptions options;
            options.depthWrite = true;
            options.depthCompare = VK_COMPARE_OP_ALWAYS;
            options.minDepth = options.maxDepth = mutated ? 0.75f + static_cast<float>(index) * 0.125f : static_cast<float>(index + 1u) * 0.25f;
            const DepthFastClearHarness::Surface surface{begin + index * Step, 0u, 0u, false, VK_FORMAT_D16_UNORM};
            check(DepthFastClearHarness::Draw(*device, shaders, surface, options) == texels, "native preflight preservation draw did not cover its attachment");
        }
        device->WaitIdle();
    };
    const auto checkValues = [&](std::span<const AgcDriver::NativeDepthSnapshot> snapshots, bool mutated) {
        check(snapshots.size() == 2u, "native preflight preservation fixture lost a native plane");
        for (unsigned index = 0u; index < 2u; ++index) {
            const auto& snapshot = snapshots[index];
            check(snapshot.target.address == begin + index * Step && snapshot.depth.size() == texels * 2u, "native preflight preservation fixture captured the wrong plane");
            const auto expected = mutated ? 0.75f + static_cast<float>(index) * 0.125f : static_cast<float>(index + 1u) * 0.25f;
            for (std::size_t pixel = 0u; pixel < texels; ++pixel) {
                std::uint16_t bits = 0u;
                std::memcpy(&bits, snapshot.depth.data() + pixel * 2u, sizeof(bits));
                check(std::abs(static_cast<float>(bits) / 65535.0f - expected) < 2.0f / 65535.0f, "native preflight preservation fixture captured an unexpected texel");
            }
        }
    };
    draw(false);
    const auto captured = device->CaptureNativeDepth();
    checkValues(captured, false);
    draw(true);
    const auto mutated = device->CaptureNativeDepth();
    checkValues(mutated, true);
    const std::array<std::uint32_t, 8> words{static_cast<std::uint32_t>(begin >> 8u), static_cast<std::uint32_t>((begin >> 40u) & 255u) | (7u << 20u) | (((Extent.width - 1u) & 3u) << 30u), ((Extent.width - 1u) >> 2u) | ((Extent.height - 1u) << 14u), 0xfacu | (9u << 28u), 0u, 0u, 0u, 0u};
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    auto held = AgcDriver::Graphics::DepthSurfaceTexture(context, words, resource, {});
    check(held != nullptr && AgcDriver::Graphics::DepthSurfaceHolds(context, resource, held.get()), "native preflight preservation did not retain its live B view");
    const auto oldView = AgcDriver::Graphics::DepthSurfaceOrderTarget(context, captured.front().target).view;
    std::array<std::byte, 32> sentinel;
    sentinel.fill(std::byte{0x5a});
    std::copy(sentinel.begin(), sentinel.end(), bytes + Bytes - sentinel.size());
    auto invalid = captured;
    invalid.back().hostFormat = VK_FORMAT_UNDEFINED;
    bool rejected = false;
    bool continued = false;
    try {
        static_cast<void>(resetMemory(reader, skipped, since, ResetFault::None, true, &invalid));
        continued = true;
    } catch (const std::runtime_error& error) {
        rejected = std::string_view(error.what()) == "native depth capture: unsupported host depth format";
    }
    check(rejected && !continued, "native preflight preservation did not reject the invalid later host format at its expected diagnostic");
    const bool guestPreserved = std::equal(sentinel.begin(), sentinel.end(), bytes + Bytes - sentinel.size());
    const bool ownerPreserved = AgcDriver::Graphics::DepthSurfaceHolds(context, resource, held.get());
    const bool sameDevice = AgcDriver::DriverDetail::DriverTestAccess::Device().get() == device.get();
    const auto after = device->CaptureNativeDepth();
    const bool contentPreserved = after.size() == mutated.size() && std::ranges::equal(after, mutated, [](const auto& left, const auto& right) { return left.target.address == right.target.address && left.depth == right.depth && left.stencil == right.stencil && left.depthOrder == right.depthOrder && left.stencilOrder == right.stencilOrder; });
    std::printf("native preflight rejected later host format: guest=%u current_B_owner=%u exact_B_contents=%u same_device=%u\n", guestPreserved ? 1u : 0u, ownerPreserved ? 1u : 0u, contentPreserved ? 1u : 0u, sameDevice ? 1u : 0u);
    check(guestPreserved && ownerPreserved && contentPreserved && sameDevice, "native replay preflight discarded current native B or guest mutations before rejecting a later invalid record");
    check(AgcDriver::Graphics::DepthSurfaceOrderTarget(context, captured.front().target).view == oldView, "native replay preflight changed the current attachment identity before rejecting the invalid record");
    device->WaitIdle();
}

struct NativeUploadFailureProbe {
    explicit NativeUploadFailureProbe(AgcDriver::Graphics::Context& context) : original(context.deviceProc) {
        check(active == nullptr && original != nullptr, "native upload failure probe has no unique resolver");
        copy = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(original(context.device, "vkCmdCopyBufferToImage"));
        wait = reinterpret_cast<PFN_vkWaitForFences>(original(context.device, "vkWaitForFences"));
        check(copy != nullptr && wait != nullptr, "native upload failure probe has no native transfer or wait function");
        fallback = original;
        fallbackCopy = copy;
        fallbackWait = wait;
        active = this;
        context.deviceProc = Resolve;
    }

    ~NativeUploadFailureProbe() { Disarm(); }
    void Disarm() { armed = false; active = nullptr; }

    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice device, const char* name) {
        if (active != nullptr && active->armed) {
            if (std::strcmp(name, "vkCmdCopyBufferToImage") == 0) {
                ++active->lookups;
                if (active->lookups == 2u) {
                    ++active->refused;
                    return nullptr;
                }
                return reinterpret_cast<PFN_vkVoidFunction>(&Copy);
            }
            if (std::strcmp(name, "vkWaitForFences") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Wait);
        }
        return (active != nullptr ? active->original : fallback)(device, name);
    }

    static VKAPI_ATTR void VKAPI_CALL Copy(VkCommandBuffer commands, VkBuffer source, VkImage destination, VkImageLayout layout, std::uint32_t count, const VkBufferImageCopy* regions) {
        auto* probe = active;
        if (probe != nullptr && probe->armed) ++probe->calls;
        (probe != nullptr ? probe->copy : fallbackCopy)(commands, source, destination, layout, count, regions);
    }

    static VKAPI_ATTR VkResult VKAPI_CALL Wait(VkDevice device, std::uint32_t count, const VkFence* fences, VkBool32 all, std::uint64_t timeout) {
        auto* probe = active;
        const auto result = (probe != nullptr ? probe->wait : fallbackWait)(device, count, fences, all, timeout);
        if (probe != nullptr && probe->armed && probe->calls == 1u && probe->refused == 0u && result == VK_SUCCESS) ++probe->completedFirstUploadWaits;
        return result;
    }

    inline static NativeUploadFailureProbe* active = nullptr;
    inline static PFN_vkGetDeviceProcAddr fallback = nullptr;
    inline static PFN_vkCmdCopyBufferToImage fallbackCopy = nullptr;
    inline static PFN_vkWaitForFences fallbackWait = nullptr;
    PFN_vkGetDeviceProcAddr original;
    PFN_vkCmdCopyBufferToImage copy = nullptr;
    PFN_vkWaitForFences wait = nullptr;
    bool armed = false;
    unsigned lookups = 0u;
    unsigned calls = 0u;
    unsigned refused = 0u;
    unsigned completedFirstUploadWaits = 0u;
};

void testNativeRepeatFailure() {
    constexpr std::size_t Step = 131072u;
    constexpr std::size_t Bytes = 16u * Step;
    constexpr VkExtent2D Extent{DepthFastClearHarness::Width, DepthFastClearHarness::Height};
    const auto texels = Extent.width * Extent.height;
    const auto begin = address(allocate(Bytes));
    auto opened = OpenVulkanTestDevice();
    check(opened != nullptr, "native partial upload failure device unavailable");
    auto device = std::shared_ptr<AgcDriver::VulkanDevice>(std::move(opened));
    AgcDriver::DriverDetail::DriverTestAccess::UseDevice(device);
    auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    const auto shaders = DepthFastClearHarness::Compile(*device);
    for (unsigned index = 0u; index < 2u; ++index) {
        DepthFastClearHarness::DrawOptions options;
        options.depthWrite = true;
        options.depthCompare = VK_COMPARE_OP_ALWAYS;
        options.minDepth = options.maxDepth = static_cast<float>(index + 1u) * 0.25f;
        const DepthFastClearHarness::Surface surface{begin + index * Step, 0u, 0u, false, VK_FORMAT_D16_UNORM};
        check(DepthFastClearHarness::Draw(*device, shaders, surface, options) == texels, "native partial upload failure source draw failed");
    }
    device->WaitIdle();
    const auto captured = device->CaptureNativeDepth();
    check(captured.size() == 2u, "native partial upload failure fixture needs two actual native planes");
    for (unsigned index = 0u; index < 2u; ++index) {
        const auto& snapshot = captured[index];
        check(snapshot.target.address == begin + index * Step && snapshot.depth.size() == texels * 2u, "native partial upload failure captured the wrong plane");
        for (std::size_t pixel = 0u; pixel < texels; ++pixel) {
            std::uint16_t bits = 0u;
            std::memcpy(&bits, snapshot.depth.data() + pixel * 2u, sizeof(bits));
            check(std::abs(static_cast<float>(bits) / 65535.0f - static_cast<float>(index + 1u) * 0.25f) < 2.0f / 65535.0f, "native partial upload failure captured an unexpected texel");
        }
    }
    NativeUploadFailureProbe probe(context);
    bool rejected = false;
    bool continued = false;
    try {
        exclusive([&] {
            check(AgcDriver::Graphics::FlushExclusion::SelectionExcludedForTest() && AgcDriver::GuestMemory::GpuMutex().HeldByThisThread(), "native partial upload failure is outside the actual guest memory transaction");
            const auto discarded = AgcDriver::DiscardNativeDepthForReplay_nid_no_patch();
            check(discarded.depthSurfaces >= 2u, "native partial upload failure did not discard its old graph");
            probe.armed = true;
            AgcDriver::Graphics::RestoreNativeDepthSurfaces(context, captured);
            continued = true;
        });
    } catch (const std::runtime_error& error) {
        rejected = std::string_view(error.what()) == "AGC graphics: missing Vulkan function: vkCmdCopyBufferToImage";
    }
    probe.Disarm();
    check(context.deviceProc(context.device, "vkCmdCopyBufferToImage") == reinterpret_cast<PFN_vkVoidFunction>(probe.copy), "native partial upload failure did not restore its native resolver fallback");
    device->WaitIdle();
    check(rejected && !continued && probe.lookups == 2u && probe.calls == 1u && probe.refused == 1u && probe.completedFirstUploadWaits >= 1u, "native partial upload failure did not reject exactly the second lookup after completing the first real upload");
    const auto active = std::ranges::count_if(captured, [&](const auto& snapshot) { return AgcDriver::Graphics::ActiveDepthExtent(device->Device(), snapshot.target.address).has_value(); });
    const auto owners = AgcDriver::Graphics::ResidentDepthOwnersOver(begin, Bytes, false);
    std::printf("native partial upload failure: lookups=%u copy_calls=%u first_upload_waits=%u refused=%u continued=%u active_planes=%zu resident_owners=%zu\n", probe.lookups, probe.calls, probe.completedFirstUploadWaits, probe.refused, continued ? 1u : 0u, static_cast<std::size_t>(active), owners);
    check(active == 0 && owners == 0u, "native restoration failure left an active partial owner graph");
    check(AgcDriver::DriverDetail::DriverTestAccess::Device().get() == device.get(), "native partial upload failure replaced the VulkanDevice object");
}

void testNativeRepeatPipelines(const std::filesystem::path& directory) {
    constexpr std::size_t Bytes = 2u << 20u;
    constexpr VkExtent2D Extent{DepthFastClearHarness::Width, DepthFastClearHarness::Height};
    auto* bytes = allocate(Bytes);
    auto* output = allocate(65536u);
    const auto begin = address(bytes);
    std::uintptr_t arena = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arena, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(begin, Bytes, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.Finish("{\"arena_base\":" + std::to_string(arena) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0u, "native warm pipeline fixture skipped guest memory");
    auto since = watchBaseline(reader, skipped);
    auto opened = OpenVulkanTestDevice();
    check(opened != nullptr, "native warm pipeline fixture device unavailable");
    auto device = std::shared_ptr<AgcDriver::VulkanDevice>(std::move(opened));
    AgcDriver::DriverDetail::DriverTestAccess::UseDevice(device);
    const auto graphics = DepthFastClearHarness::Compile(*device);
    const DepthFastClearHarness::Surface surface{begin, 0u, 0u, false, VK_FORMAT_D16_UNORM};
    DepthFastClearHarness::DrawOptions options;
    options.depthWrite = true;
    options.depthCompare = VK_COMPARE_OP_ALWAYS;
    options.minDepth = options.maxDepth = 0.25f;
    check(DepthFastClearHarness::Draw(*device, graphics, surface, options) == Extent.width * Extent.height, "native warm pipeline initial depth draw failed");
    const auto captured = device->CaptureNativeDepth();
    check(captured.size() == 1u && !captured.front().depth.empty(), "native warm pipeline fixture did not capture its native plane");
    const auto destination = address(output);
    const auto words = std::span(NarrowConstantStoreFixture::Code);
    const std::array<std::uint32_t, 2> user{static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u)};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{address(words.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{NarrowConstantStoreFixture::Threads, 1u, 1u}, 0u, {false, false, false}, false, 1u, {}};
    ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Compute, address(words.data()), words, 0u, {}}, {32u, 0u, user, compute, std::nullopt, std::nullopt, memory}, device->Target(), {0u, 0u, 0u, 128u}};
    request.useCache = false;
    const auto shader = ShaderRecompiler::Recompile(request);
    const auto execute = [&] {
        std::fill_n(output, NarrowConstantStoreFixture::Threads * NarrowConstantStoreFixture::LaneBytes, std::byte{NarrowConstantStoreFixture::Fill});
        device->Dispatch(shader, 1u, 1u, 1u, {}, request.shader.codeAddress);
        device->WaitIdle();
        for (std::size_t offset = 0u; offset < NarrowConstantStoreFixture::Threads * NarrowConstantStoreFixture::LaneBytes; ++offset) check(output[offset] == std::byte{NarrowConstantStoreFixture::ExpectedLane[offset % NarrowConstantStoreFixture::LaneBytes]}, "native warm pipeline dispatch did not perform the real expected stores");
    };
    execute();
    device->WaitForPipelineJournalReplay();
    const auto warmed = device->ComputePipelinesCreated();
    check(warmed != 0u && device->HasComputePipeline(shader), "native warm pipeline fixture did not build and retain its actual compute operation");
    for (unsigned pass = 1u; pass < 3u; ++pass) {
        options.minDepth = options.maxDepth = 0.75f;
        check(DepthFastClearHarness::Draw(*device, graphics, surface, options) == Extent.width * Extent.height, "native warm pipeline window mutation failed");
        static_cast<void>(resetMemory(reader, skipped, since, ResetFault::None, true, &captured));
        check(AgcDriver::DriverDetail::DriverTestAccess::Device().get() == device.get() && device->HasComputePipeline(shader) && device->ComputePipelinesCreated() == warmed, "native repeated reset discarded or rebuilt a warmed compute pipeline");
        checkNativeRepeatContents(captured, device->CaptureNativeDepth());
        execute();
        check(device->HasComputePipeline(shader) && device->ComputePipelinesCreated() == warmed, "repeating the warmed compute operation after native reset rebuilt its pipeline");
        std::printf("native warm pipeline pass %u: retained=1 created=%llu warmed=%llu\n", pass + 1u, static_cast<unsigned long long>(device->ComputePipelinesCreated()), static_cast<unsigned long long>(warmed));
    }
}

void testNativeRepeatHtileOrphan(const std::filesystem::path& directory) {
    constexpr std::size_t Block = 65536u;
    constexpr std::size_t CapturedBytes = 2u * Block;
    constexpr VkExtent2D Extent{DepthFastClearHarness::Width, DepthFastClearHarness::Height};
    constexpr float CapturedDepth = 0.25f;
    const auto texels = Extent.width * Extent.height;
    auto* bytes = allocate(CapturedBytes);
    const auto begin = address(bytes);
    const auto separatePair = [&](std::uint64_t previous) {
        constexpr std::uint64_t Gap = 2u << 20u;
        check(previous <= std::numeric_limits<std::uint64_t>::max() - Gap, "orphan HTILE independent allocation hint overflows");
        auto* pair = static_cast<std::byte*>(GuestArena::GuestArenaAllocateAtOrAbove_nid_postfix(previous + Gap, CapturedBytes, Block));
        check(pair != nullptr && address(pair) >= previous + Gap, "orphan HTILE independent allocation is not separated from its predecessor");
        GuestArena::GuestArenaCommit_nid_postfix(pair, CapturedBytes, ReadWrite, CapturedBytes);
#ifdef __linux__
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(pair, CapturedBytes);
#endif
        GuestAllocations::Mutation mutation;
        mutation.Add(pair, CapturedBytes, true, true, true);
        return pair;
    };
    auto* unnotedBytes = separatePair(begin);
    auto* notedBytes = separatePair(address(unnotedBytes));
    auto* outsideBytes = separatePair(address(notedBytes));
    for (auto* pair : std::array{bytes, unnotedBytes, notedBytes, outsideBytes}) {
        for (std::size_t offset = 0u; offset < Block; offset += sizeof(float)) std::memcpy(pair + offset, &CapturedDepth, sizeof(float));
        std::fill_n(pair + Block, Block, std::byte{0xff});
    }
    std::printf("orphan HTILE independent allocation pairs: tested=%llu unnoted=%llu noted=%llu outside=%llu bytes=%zu\n", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(address(unnotedBytes)), static_cast<unsigned long long>(address(notedBytes)), static_cast<unsigned long long>(address(outsideBytes)), CapturedBytes);
    const std::vector<std::byte> expected(bytes, bytes + CapturedBytes);
    std::uintptr_t arena = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arena, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(begin, CapturedBytes, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.WriteSnapshot(begin, expected);
    writer.Finish("{\"arena_base\":" + std::to_string(arena) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0u && std::equal(expected.begin(), expected.end(), bytes), "orphan HTILE fixture did not restore captured raw depth and expanded metadata");
    auto since = watchBaseline(reader, skipped);
    auto opened = OpenVulkanTestDevice();
    check(opened != nullptr, "orphan HTILE fixture device unavailable");
    auto device = std::shared_ptr<AgcDriver::VulkanDevice>(std::move(opened));
    AgcDriver::DriverDetail::DriverTestAccess::UseDevice(device);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    const auto shaders = DepthFastClearHarness::Compile(*device);
    const auto swizzle = AgcDriver::Graphics::XorSwizzleMode(AgcDriver::Graphics::TextureTileMode::kZ64KBX);
    const auto firstHtileBind = [&](std::uint64_t depth) {
        AgcDriver::Graphics::GuestTextureResource resource{};
        resource.baseAddress = depth;
        resource.width = Extent.width;
        resource.height = Extent.height;
        resource.mipCount = resource.allocatedMipCount = 1u;
        resource.tileMode = AgcDriver::Graphics::TextureTileMode::kZ64KBX;
        resource.dimension = AgcDriver::Graphics::TextureDimension::k2D;
        resource.format = 22u;
        resource.dstSelX = 4u;
        resource.dstSelY = 5u;
        resource.dstSelZ = 6u;
        resource.dstSelW = 7u;
        std::shared_ptr<AgcDriver::Graphics::StorageTexture> storage;
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            storage = AgcDriver::Graphics::CachedStorageSurface(context, resource);
        }
        const auto planeBytes = AgcDriver::Graphics::DepthSliceBytes(Extent, 4u);
        check(storage != nullptr && storage->Cached() && storage->Descriptor().width == Extent.width && storage->Descriptor().height == Extent.height && AgcDriver::Graphics::StorageTexture::FindLive(depth, planeBytes) == storage, "orphan HTILE fixture did not register the raw captured A storage image for attachment import");
        std::printf("orphan HTILE raw import: address=%llu cached=1 bytes=%llu live_match=1\n", static_cast<unsigned long long>(depth), static_cast<unsigned long long>(planeBytes));
        AgcDriver::Graphics::DepthTarget imported{depth, 0u, Extent, VK_FORMAT_D32_SFLOAT, 1.0f, 0u};
        imported.depthSwizzle = swizzle;
        static_cast<void>(AgcDriver::Graphics::DepthSurfaceView(context, imported, false, false));
        DepthFastClearHarness::DrawOptions options;
        options.depthCompare = VK_COMPARE_OP_EQUAL;
        options.minDepth = options.maxDepth = CapturedDepth;
        const DepthFastClearHarness::Surface plain{depth, 0u, 0u, false, VK_FORMAT_D32_SFLOAT, 0u, 0u, swizzle};
        check(DepthFastClearHarness::Draw(*device, shaders, plain, options) == texels, "orphan HTILE fixture did not establish raw captured A before the first metadata bind");
        const DepthFastClearHarness::Surface htiled{depth, 0u, depth + Block, false, VK_FORMAT_D32_SFLOAT, 0u, 0u, swizzle};
        const auto captured = DepthFastClearHarness::Draw(*device, shaders, htiled, options);
        options.minDepth = options.maxDepth = 1.0f;
        const auto cleared = DepthFastClearHarness::Draw(*device, shaders, htiled, options);
        return std::pair{captured, cleared};
    };
    const auto unnoted = firstHtileBind(address(unnotedBytes));
    check(unnoted.first == texels && unnoted.second == 0u, "orphan HTILE unnoted control did not retain raw captured A");
    check(!AgcDriver::Graphics::NoteHtileDepthClear(address(notedBytes) + Block, DepthFastClearHarness::HtileBytes), "orphan HTILE noted control unexpectedly found a native completion owner");
    const auto noted = firstHtileBind(address(notedBytes));
    check(noted.first == 0u && noted.second == texels, "orphan HTILE noted control did not apply remembered clear B at its first metadata bind");
    std::printf("orphan HTILE independent controls: unnoted_A=%u unnoted_B=%u noted_A=%u noted_B=%u\n", unnoted.first, unnoted.second, noted.first, noted.second);
    check(!AgcDriver::Graphics::ActiveDepthExtent(device->Device(), begin).has_value() && AgcDriver::Graphics::ResidentDepthOwnersOver(begin, CapturedBytes, false) == 0u, "orphan HTILE tested range already has a resident owner before reset");
    check(!AgcDriver::Graphics::NoteHtileDepthClear(begin + Block, DepthFastClearHarness::HtileBytes), "orphan HTILE tested note unexpectedly found a native completion owner");
    check(!AgcDriver::Graphics::NoteHtileDepthClear(address(outsideBytes) + Block, DepthFastClearHarness::HtileBytes), "orphan HTILE outside-range note unexpectedly found a native completion owner");
    Runs changed;
    static_cast<void>(changedSince(begin, CapturedBytes, since, changed));
    Runs owned;
    std::exception_ptr failure;
    AgcDriverGpuOwnedRanges_nid_no_patch(begin, CapturedBytes, owned, &failure);
    rethrowDriverFailure(failure);
    const auto equal = std::equal(expected.begin(), expected.end(), bytes);
    std::printf("orphan HTILE pre-reset isolation: changed=%zu owned=%zu byte_equal=%u tested_begin=%llu tested_bytes=%zu\n", changed.size(), owned.size(), equal ? 1u : 0u, static_cast<unsigned long long>(begin), CapturedBytes);
    for (const auto& [first, last] : changed) std::printf("orphan HTILE changed span: begin=%llu end=%llu bytes=%llu\n", static_cast<unsigned long long>(first), static_cast<unsigned long long>(last), static_cast<unsigned long long>(last - first));
    for (const auto& [first, length] : owned) std::printf("orphan HTILE owned span: begin=%llu bytes=%llu\n", static_cast<unsigned long long>(first), static_cast<unsigned long long>(length));
    check(changed.empty(), "orphan HTILE independent controls recorded a change in the tested captured range");
    check(owned.empty(), "orphan HTILE independent controls created a GPU owner over the tested captured range");
    check(equal, "orphan HTILE independent controls changed captured guest bytes before reset");
    const std::vector<AgcDriver::NativeDepthSnapshot> emptyNative;
    const auto reset = resetMemory(reader, skipped, since, ResetFault::None, true, &emptyNative);
    check(reset.compared == 0u && reset.rewritten == 0u && std::equal(expected.begin(), expected.end(), bytes), "orphan HTILE reset test became covered by generic changed-page retirement or altered captured bytes");
    const auto restored = firstHtileBind(begin);
    const auto outside = firstHtileBind(address(outsideBytes));
    std::printf("orphan HTILE reset: changed=0 owned=0 compared=%llu rewritten=%llu restored_A=%u restored_B=%u outside_A=%u outside_B=%u\n", static_cast<unsigned long long>(reset.compared), static_cast<unsigned long long>(reset.rewritten), restored.first, restored.second, outside.first, outside.second);
    check(restored.first == texels && restored.second == 0u, "native replay reset retained an orphan HTILE clear over unchanged captured memory");
    check(outside.first == 0u && outside.second == texels, "native replay reset globally erased an HTILE note outside captured ranges");
}

void testMemoryOrder(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 64u * 1024u * 1024u;
    auto* bytes = allocate(Size);
    const std::vector<std::byte> first(Size, std::byte{0x11});
    const std::vector<std::byte> second(4096, std::byte{0x22});
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(bytes), first);
    writer.WriteDelta(2, 0, address(bytes + Size - second.size()), second);
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.release = false;
    Replay replay{options, reader};
    replay.recording = true;
    collectGroups(replay);
    const std::array calls{memoryCall(1, 1), memoryCall(2, 2)};
    replay.runWindow(calls);
    check(std::all_of(bytes, bytes + Size - second.size(), [](std::byte value) { return value == std::byte{0x11}; }), "first delta was not restored");
    check(std::equal(second.begin(), second.end(), bytes + Size - second.size()), "an earlier delta overwrote the later thread's delta");
}

void testReusedMemory(const std::filesystem::path& directory) {
#if defined(_WIN32) || defined(__linux__)
    constexpr std::size_t Size = 2 * FrameCaptureWriter::BlockBytes;
    auto* bytes = allocate(Size);
    auto* cpu = allocate(FrameCaptureWriter::BlockBytes, false);
    std::fill_n(bytes, Size, std::byte{0x7f});
    std::fill_n(cpu, FrameCaptureWriter::BlockBytes, std::byte{0x55});
    std::vector<std::byte> expected(Size);
    std::fill(expected.begin() + FrameCaptureWriter::BlockBytes, expected.end(), std::byte{0x22});
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(bytes), Size, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.WriteRange(address(cpu), FrameCaptureWriter::BlockBytes, FrameRangeKind::Unwatched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable);
    check(writer.WriteSnapshot(address(bytes), expected) == FrameCaptureWriter::BlockBytes, "zero snapshot block was not elided");
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(std::equal(expected.begin(), expected.end(), bytes), "reused GPU memory did not restore zero and nonzero snapshot blocks");
    check(skipped.bytes == FrameCaptureWriter::BlockBytes, "unexpected skipped memory");
    check(std::all_of(cpu, cpu + FrameCaptureWriter::BlockBytes, [](std::byte value) { return value == std::byte{0x55}; }), "reused CPU memory was overwritten");
#endif
}

void testSharedPage(const std::filesystem::path& directory) {
#ifdef __linux__
    constexpr std::size_t Page = 65536;
    auto* page = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(Page, Page));
    check(page != nullptr, "guest reservation failed");
    GuestArena::GuestArenaRelease_nid_postfix(page, Page);
    const std::vector<std::byte> gpu(0x20, std::byte{0x3c});
    const std::vector<std::byte> cpu(0x40, std::byte{0x5a});
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(page + 0x20), gpu.size(), FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.WriteRange(address(page + 0x40), cpu.size(), FrameRangeKind::Unwatched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable);
    writer.WriteSnapshot(address(page + 0x20), gpu);
    writer.WriteSnapshot(address(page + 0x40), cpu);
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0, "a range sharing a page with an earlier one was skipped");
    check(std::equal(gpu.begin(), gpu.end(), page + 0x20) && std::equal(cpu.begin(), cpu.end(), page + 0x40), "ranges sharing a page were not restored");
    check(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address(page), static_cast<std::size_t>(sysconf(_SC_PAGESIZE))), "a watched range inside a page left the page that holds it unwatched");
#else
    static_cast<void>(directory);
#endif
}

void testSubPageResets(const std::filesystem::path& directory) {
    constexpr std::size_t Block = 65536;
    auto* const block = allocate(Block);
    auto* const left = block + 0x100;
    auto* const right = block + 0x800;
    const std::vector<std::byte> leftBytes(0x600, std::byte{0x11});
    const std::vector<std::byte> rightBytes(0x400, std::byte{0x22});
    const auto access = FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu;
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(left), leftBytes.size(), FrameRangeKind::Watched, ReadWrite, access);
    writer.WriteRange(address(right), rightBytes.size(), FrameRangeKind::Watched, ReadWrite, access);
    writer.WriteSnapshot(address(left), leftBytes);
    writer.WriteSnapshot(address(right), rightBytes);
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0, "a range sharing a page with another was skipped");
    check(AgcDriver::GuestMemory::Watched(address(block), 4096), "the page the two ranges share is not write-watched");
    auto since = watchBaseline(reader, skipped);
    const auto reset = [&] { return resetMemory(reader, skipped, since, ResetFault::None, true); };
    const auto restored = [&] { return std::equal(leftBytes.begin(), leftBytes.end(), left) && std::equal(rightBytes.begin(), rightBytes.end(), right); };
    const auto idle = reset();
    check(idle.compared == 0 && idle.rewritten == 0 && idle.workers == 0, "a reset after a pass that wrote nothing compared or rewrote memory, or counted threads that compared nothing");
    for (int pass = 0; pass < 4; ++pass) {
        const bool both = pass % 2 == 1;
        std::memset(left, 0x40 + pass, 0x10);
        if (both) std::memset(right + 0x3f0, 0x50 + pass, 0x10);
        const auto result = reset();
        check(restored(), "a reset did not restore two ranges sharing one page");
        check(result.compared == leftBytes.size() + rightBytes.size(), "a reset did not compare the two ranges of the written page");
        check(result.rewritten == leftBytes.size() + (both ? rightBytes.size() : 0u), "a reset rewrote a range sharing a page with a written one, or missed a written range");
    }
    const auto settled = reset();
    check(settled.compared == 0 && settled.rewritten == 0, "the writes a reset made counted as writes of the next pass");
    AgcDriver::GuestMemory::SetChangedSinceWalkFailureForTest(true);
    const auto unwalked = reset();
    AgcDriver::GuestMemory::SetChangedSinceWalkFailureForTest(false);
    check(unwalked.compared == leftBytes.size() + rightBytes.size() && unwalked.rewritten == 0, "a reset whose write walk failed did not compare the memory it could not walk");
    bool refused = false;
    try {
        static_cast<void>(resetMemory(reader, skipped, since, ResetFault::Remap, false));
    } catch (const std::runtime_error& error) {
        refused = std::string_view(error.what()).find("mapped or unmapped during the reset") != std::string_view::npos;
    }
    check(refused, "a reset accepted a mapping change made during it");
}

void testPartialWalk(const std::filesystem::path& directory) {
    constexpr std::size_t Block = 65536;
    constexpr std::size_t Bytes = 4 * Block;
    auto* const bytes = allocate(Bytes);
    std::vector<std::byte> captured(Bytes);
    for (std::size_t index = 0; index < Bytes; ++index) captured[index] = static_cast<std::byte>(index * 7 + 3);
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(bytes), Bytes, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.WriteSnapshot(address(bytes), captured);
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0, "the captured range was skipped");
    auto since = watchBaseline(reader, skipped);
    const auto reset = [&] { return resetMemory(reader, skipped, since, ResetFault::None, true); };
    const auto restored = [&] { return std::equal(captured.begin(), captured.end(), bytes); };
    check(reset().compared == 0, "a reset after a pass that wrote nothing compared memory");
    bytes[0] = ~bytes[0];
    bytes[3 * Block + 0x123] = ~bytes[3 * Block + 0x123];
    AgcDriver::GuestMemory::SetChangedSinceWalkFailureForTest(true);
    ResetResult cut;
    try {
        cut = reset();
    } catch (...) {
        AgcDriver::GuestMemory::SetChangedSinceWalkFailureForTest(false);
        throw;
    }
    AgcDriver::GuestMemory::SetChangedSinceWalkFailureForTest(false);
    check(restored(), "a reset whose write walks failed after collecting half of the range did not restore it");
    check(cut.compared == Bytes && cut.rewritten == 2 * 4096u, "a reset whose write walks failed after collecting half of the range did not compare it whole and rewrite exactly the two written pages");
    bytes[Block + 5] = ~bytes[Block + 5];
    const auto next = reset();
    check(restored() && next.rewritten == 4096u, "the reset after one whose write walks failed partway did not rewrite exactly the page written since");
    check(reset().compared == 0, "the writes of a reset after a partial walk counted as writes of the next pass");
}

void testLargeRange(const std::filesystem::path& directory) {
    constexpr std::size_t Bytes = 2 * ResetPiece + 65536;
    auto* const bytes = allocate(Bytes);
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(bytes), Bytes, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0, "the captured range was skipped");
    auto since = watchBaseline(reader, skipped);
    std::memset(bytes, 0x5a, Bytes);
    const auto result = resetMemory(reader, skipped, since, ResetFault::None, true);
    check(std::all_of(bytes, bytes + Bytes, [](std::byte value) { return value == std::byte{0}; }), "a reset of one large range did not restore it");
    check(result.rewritten == Bytes, "a reset of one large written range did not rewrite all of it");
    check(std::thread::hardware_concurrency() < 2 || result.workers >= 2, "a reset of one range larger than two pieces compared and rewrote it on fewer than two threads");
}

std::atomic<std::uint64_t> flushLandsAt{0};
void (*flushChained)(std::uint64_t, std::size_t) = nullptr;

void landRecordedStore(std::uint64_t address, std::size_t bytes) {
    auto target = flushLandsAt.load(std::memory_order_acquire);
    if (target != 0 && address <= target && target < address + bytes && flushLandsAt.compare_exchange_strong(target, 0)) *reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(target)) = 0x3f000000u;
    if (flushChained != nullptr) flushChained(address, bytes);
}

bool testFlushedOwner(const std::filesystem::path& directory) {
    std::unique_ptr<AgcDriver::VulkanDevice> device;
    try {
        device = std::make_unique<AgcDriver::VulkanDevice>();
    } catch (const AgcDriver::VulkanUnavailable& error) {
        if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
        std::printf("skipped, no usable Vulkan device: %s\n", error.what());
        return false;
    }
    constexpr std::size_t Bytes = 4u * 65536u;
    auto* const block = allocate(Bytes);
    const std::vector<std::byte> captured(Bytes, std::byte{0});
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(block), Bytes, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.WriteSnapshot(address(block), captured);
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0, "the captured range was skipped");
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    AgcDriver::Graphics::DepthSurfaceView(context, {address(block), 0, {64, 64}, VK_FORMAT_D32_SFLOAT, 1.0f, 0}, false);
    device->WaitIdle();
    auto since = watchBaseline(reader, skipped);
    std::exception_ptr failure;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> owned;
    AgcDriverGpuOwnedRanges_nid_no_patch(address(block), Bytes, owned, &failure);
    rethrowDriverFailure(failure);
    check(owned.empty(), "a depth surface that holds no results only on the GPU is among the GPU-owned ranges");
    std::size_t owners = 0;
    AgcDriverResidentOwnersOver_nid_no_patch(address(block), 4096u, false, &owners, &failure);
    rethrowDriverFailure(failure);
    check(owners == 1, "the depth surface over the captured range is not resident");
    flushChained = AgcDriver::GuestMemory::CurrentFlushHook();
    AgcDriver::GuestMemory::SetFlushHook(&landRecordedStore);
    flushLandsAt.store(address(block) + 0x100u, std::memory_order_release);
    ResetResult result;
    try {
        result = resetMemory(reader, skipped, since, ResetFault::None, true);
    } catch (...) {
        AgcDriver::GuestMemory::SetFlushHook(flushChained);
        throw;
    }
    AgcDriver::GuestMemory::SetFlushHook(flushChained);
    check(flushLandsAt.load() == 0, "the reset's flush did not land the recorded store");
    check(result.flushed != 0, "the reset did not add the page its flush wrote to the pages it compares");
    check(result.retired.depthSurfaces == 1, "the reset did not retire the depth surface under the page its flush wrote");
    AgcDriverResidentOwnersOver_nid_no_patch(address(block), 4096u, false, &owners, &failure);
    rethrowDriverFailure(failure);
    check(owners == 0, "a depth surface under a page the reset's flush wrote stayed resident");
    check(std::all_of(block, block + Bytes, [](std::byte value) { return value == std::byte{0}; }), "the reset did not restore the page its flush wrote");
    AgcDriver::Graphics::ClearDepthSurfaces(context.device);
    return true;
}

void imageBarrier(const AgcDriver::Graphics::Context& context, VkCommandBuffer commands, VkImage image, VkImageLayout from, VkAccessFlags source, VkAccessFlags destination) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = source;
    barrier.dstAccessMask = destination;
    barrier.oldLayout = from;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

std::vector<std::uint32_t> resolvedTexels(const AgcDriver::Graphics::Context& context, VkImage image, std::uint32_t side) {
    AgcDriver::Graphics::ColorTarget single{};
    single.extent = {side, side};
    single.format = VK_FORMAT_R8G8B8A8_UNORM;
    single.elementBytes = 4;
    single.samples = 1;
    single.bytes = static_cast<std::uint64_t>(side) * side * 4u;
    AgcDriver::Graphics::RenderTarget resolved(context, single, false);
    AgcDriver::Graphics::Buffer readback(context, static_cast<std::size_t>(single.bytes), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    AgcDriver::Graphics::CommandBatch batch(context);
    imageBarrier(context, batch.Handle(), image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    imageBarrier(context, batch.Handle(), resolved.Image(), VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageResolve region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstSubresource = region.srcSubresource;
    region.extent = {side, side, 1};
    context.Function<PFN_vkCmdResolveImage>("vkCmdResolveImage")(batch.Handle(), image, VK_IMAGE_LAYOUT_GENERAL, resolved.Image(), VK_IMAGE_LAYOUT_GENERAL, 1, &region);
    imageBarrier(context, batch.Handle(), resolved.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy copy{};
    copy.imageSubresource = region.dstSubresource;
    copy.imageExtent = region.extent;
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(batch.Handle(), resolved.Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
    AgcDriver::Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    batch.SubmitAndWait();
    readback.Invalidate();
    std::vector<std::uint32_t> texels(static_cast<std::size_t>(side) * side);
    std::memcpy(texels.data(), readback.Bytes().data(), texels.size() * sizeof(std::uint32_t));
    return texels;
}

bool testUntouchedGpuOwner(const std::filesystem::path& directory) {
    std::unique_ptr<AgcDriver::VulkanDevice> device;
    try {
        device = std::make_unique<AgcDriver::VulkanDevice>();
    } catch (const AgcDriver::VulkanUnavailable& error) {
        if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
        std::printf("skipped, no usable Vulkan device: %s\n", error.what());
        return false;
    }
    constexpr std::uint32_t Side = 256;
    constexpr std::size_t Bytes = 1u << 20u;
    constexpr std::uint32_t Drawn = 0x40404040u;
    auto* const block = allocate(Bytes);
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(block), Bytes, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0, "the captured range was skipped");
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    AgcDriver::Graphics::ColorTarget target{};
    target.address = target.surfaceAddress = address(block);
    target.extent = target.surfaceExtent = {Side, Side};
    target.format = VK_FORMAT_R8G8B8A8_UNORM;
    target.elementBytes = 4;
    target.samples = 4;
    target.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
    target.bytes = AgcDriver::Graphics::ColorTargetLayout(Side, Side, target.tileMode, target.elementBytes).Bytes();
    check(AgcDriver::Graphics::MultisampledColorBytes(target) == Bytes, "the multisampled colour target does not cover the captured range exactly");
    const auto bind = [&] {
        VkImage image = VK_NULL_HANDLE;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        check(AgcDriver::Graphics::MultisampleTargetView(context, target, &image) != VK_NULL_HANDLE && image != VK_NULL_HANDLE, "the multisampled colour target has no view");
        device->SubmitRecorded(true);
        device->WaitIdle();
        return image;
    };
    const auto drawn = bind();
    {
        AgcDriver::Graphics::CommandBatch batch(context);
        imageBarrier(context, batch.Handle(), drawn, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        VkClearColorValue clear{};
        clear.float32[0] = clear.float32[1] = clear.float32[2] = clear.float32[3] = 64.0f / 255.0f;
        const VkImageSubresourceRange whole{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(batch.Handle(), drawn, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &whole);
        batch.SubmitAndWait();
    }
    const auto before = resolvedTexels(context, drawn, Side);
    check(std::ranges::all_of(before, [](std::uint32_t texel) { return texel == Drawn; }), "the colour drawn through Vulkan did not reach every sample");
    auto since = watchBaseline(reader, skipped);
    Runs changed;
    static_cast<void>(changedSince(address(block), Bytes, since, changed));
    check(changed.empty(), "a page under the GPU-only owner was written although only Vulkan drew into it");
    std::size_t owners = 0;
    std::exception_ptr failure;
    AgcDriverResidentOwnersOver_nid_no_patch(address(block), Bytes, true, &owners, &failure);
    rethrowDriverFailure(failure);
    check(owners == 1, "the multisampled colour target does not hold its results only on the GPU");
    const auto result = resetMemory(reader, skipped, since, ResetFault::None, false);
    check(result.retired.color >= 1, "the reset did not retire a GPU-only owner over pages nobody wrote");
    check(result.compared >= Bytes && result.rewritten == 0, "the reset did not compare the GPU-only owner's pages, or rewrote pages that matched the capture");
    AgcDriverResidentOwnersOver_nid_no_patch(address(block), Bytes, true, &owners, &failure);
    rethrowDriverFailure(failure);
    check(owners == 0, "a GPU-only owner over pages nobody wrote survived the reset");
    check(std::all_of(block, block + Bytes, [](std::byte value) { return value == std::byte{0}; }), "the reset changed the captured bytes under the GPU-only owner");
    const auto after = resolvedTexels(context, bind(), Side);
    check(std::ranges::all_of(after, [](std::uint32_t texel) { return texel == 0u; }), "the target bound again after the reset kept the samples drawn in the earlier pass");
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    AgcDriver::Graphics::ClearMultisampleTargets(context.device);
    return true;
}

void testWatchedExisting(const std::filesystem::path& directory) {
#ifdef __linux__
    constexpr std::size_t Size = 3u * 65536u;
    auto* bytes = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(Size, 65536));
    check(bytes != nullptr, "guest allocation failed");
    GuestArena::GuestArenaCommit_nid_postfix(bytes, Size, 0x04, Size);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(bytes, Size, true, true, true);
    }
    const auto watched = bytes + 65536 + 0x30;
    const std::size_t watchedBytes = 0x100;
    check(!GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address(watched), watchedBytes), "the writable mapping was already write-watched");
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(watched), watchedBytes, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == 0, "a watched range on a writable mapping was skipped");
    check(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address(watched), watchedBytes), "a watched range on an already writable mapping was not write-watched");
#else
    static_cast<void>(directory);
#endif
}

void testHostNeighbors(const std::filesystem::path& directory) {
#ifdef __linux__
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* mapped = mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    check(mapped != MAP_FAILED, "host allocation failed");
    auto* occupied = static_cast<std::byte*>(mapped);
    auto* available = occupied + page;
    check(munmap(available, page) == 0, "host page release failed");
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    FrameCaptureWriter writer(directory);
    writer.WriteRange(address(occupied), page, FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable);
    writer.WriteRange(address(available), page, FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
    writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    const FrameCaptureReader reader(directory);
    const auto skipped = restoreMemory(reader);
    check(skipped.bytes == page, "an occupied CPU host range was not the only range skipped");
#else
    static_cast<void>(directory);
#endif
}

void testUnwatched() {
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    try {
        recorder->FrameBoundary();
    } catch (const std::runtime_error& error) {
        check(std::string_view(error.what()).find("write watch is unavailable") != std::string_view::npos, "a capture without the write watch failed for another reason");
        return;
    }
    check(false, "a capture started without the write watch");
}

std::atomic<std::uint32_t*> pendingResult{nullptr};

void storePendingResult(std::uint64_t begin, std::size_t bytes) {
    auto* const result = pendingResult.load();
    if (result == nullptr || begin >= address(result) + sizeof(std::uint32_t) || address(result) >= begin + bytes) return;
    *result = 1;
    pendingResult = nullptr;
}

std::uint64_t groupBeforeSuspend(const FrameCaptureReader& reader, std::size_t index) {
    const FrameCall* previous = nullptr;
    for (const auto& call : reader.Calls()) {
        if (call.kind == FrameCallKind::Suspend) {
            if (index == 0) return previous != nullptr && previous->kind == FrameCallKind::Memory ? previous->values.at(0) : 0;
            --index;
        }
        previous = &call;
    }
    return 0;
}

std::vector<std::uint64_t> deltaGroupsHolding(const FrameCaptureReader& reader, const std::byte* page) {
    std::vector<std::uint64_t> groups;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind != FrameMemoryKind::Delta || chunk.address >= address(page) + 4096 || address(page) >= chunk.address + chunk.bytes) continue;
        if (std::find(groups.begin(), groups.end(), chunk.group) == groups.end()) groups.push_back(chunk.group);
    }
    return groups;
}

void testPagesCollectedOnce(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0});
    auto* x = bytes + 2 * FrameCaptureWriter::BlockBytes;
    auto* y = bytes + 4 * FrameCaptureWriter::BlockBytes;
    auto* shader = reinterpret_cast<Shader*>(x + 2 * Page);
    *shader = {};
    shader->file_header = 0x34333231u;
    shader->version = 0x18u;
    shader->header_size = sizeof(Shader);
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    std::fill_n(x, Page, std::byte{0x31});
    recorder->Suspend([] {});
    std::fill_n(x + FrameCaptureWriter::BlockBytes, Page, std::byte{0x4d});
    recorder->RegisterShader(shader);
    recorder->RegisterShader(shader);
    auto* late = allocate(Size);
    std::fill_n(late, Size, std::byte{0});
    auto* lateSource = late + 2 * FrameCaptureWriter::BlockBytes;
    std::fill_n(lateSource, Page, std::byte{0x57});
    std::fill_n(y, Page, std::byte{0x65});
    recorder->Suspend([] {});
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    const auto first = groupBeforeSuspend(reader, 0);
    const auto second = groupBeforeSuspend(reader, 1);
    check(first != 0 && second != 0, "the CPU writes before the first two suspend points are not in memory deltas");
    std::string failures;
    const auto expectOnce = [&](const std::byte* page, std::uint64_t group, const char* name) {
        const auto groups = deltaGroupsHolding(reader, page);
        if (groups == std::vector<std::uint64_t>{group}) return;
        failures += std::string(failures.empty() ? "" : "; ") + name + (groups.empty() ? " is in no memory delta" : " is in memory delta groups");
        for (const auto held : groups) failures += " " + std::to_string(held);
        failures += ", not only in group " + std::to_string(group);
    };
    expectOnce(x, first, "the page written before the first suspend point");
    expectOnce(y, second, "the page written before the second suspend point");
    expectOnce(lateSource, second, "the written page of the range committed during the capture");
    const auto& calls = reader.Calls();
    const auto registration = std::find_if(calls.begin(), calls.end(), [](const FrameCall& call) { return call.kind == FrameCallKind::RegisterShader; });
    if (registration == calls.end() || registration + 1 == calls.end() || (registration + 1)->kind != FrameCallKind::RegisterShader) failures += std::string(failures.empty() ? "" : "; ") + "the second registration of the shader collected its blocks again";
    if (!failures.empty()) throw std::runtime_error(failures);
}

void testPendingResultRead(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0});
    auto* page = bytes + 2 * FrameCaptureWriter::BlockBytes;
    auto* result = reinterpret_cast<std::uint32_t*>(page + Page - sizeof(std::uint32_t));
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    struct FlushHookReset {
        ~FlushHookReset() {
            AgcDriver::GuestMemory::SetFlushHook(nullptr);
            pendingResult = nullptr;
        }
    } reset;
    AgcDriver::GuestMemory::SetFlushHook(&storePendingResult);
    std::fill_n(page, Page - sizeof(std::uint32_t), std::byte{0x3c});
    pendingResult = result;
    recorder->Suspend([] {});
    auto* late = allocate(Size);
    std::fill_n(late, Size, std::byte{0});
    auto* latePage = late + 2 * FrameCaptureWriter::BlockBytes;
    auto* lateResult = reinterpret_cast<std::uint32_t*>(latePage + Page - sizeof(std::uint32_t));
    std::fill_n(latePage, Page - sizeof(std::uint32_t), std::byte{0x3c});
    pendingResult = lateResult;
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    const FrameCaptureReader reader(directory);
    const auto first = groupBeforeSuspend(reader, 0);
    const auto second = groupBeforeSuspend(reader, 1);
    check(first != 0 && second != 0, "the CPU writes before the suspend points are not in memory deltas");
    std::string failures;
    const auto expectStored = [&](const std::uint32_t* word, std::uint64_t group, const char* name) {
        std::optional<std::uint32_t> held;
        for (const auto& chunk : reader.Chunks()) {
            if (chunk.kind != FrameMemoryKind::Delta || chunk.group != group || chunk.address > address(word) || address(word) + sizeof(std::uint32_t) > chunk.address + chunk.bytes) continue;
            const auto data = reader.Read(chunk);
            std::uint32_t value = 0;
            std::memcpy(&value, data.data() + (address(word) - chunk.address), sizeof(value));
            held = value;
        }
        if (held == 1u) return;
        failures += std::string(failures.empty() ? "" : "; ") + name + (held ? " read the page before the result pending over it was stored: it holds " + std::to_string(*held) + ", not 1" : std::string(" does not hold the page the CPU wrote"));
    };
    expectStored(result, first, "the memory delta before the first suspend point");
    expectStored(lateResult, second, "the memory delta that added the range committed during the capture");
    if (!failures.empty()) throw std::runtime_error(failures);
}

void testPartlyCoveredBlock(const std::filesystem::path& directory) {
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(4 * Page);
    std::fill_n(bytes, 4 * Page, std::byte{0x2b});
    auto* shader = reinterpret_cast<Shader*>(bytes + 2 * Page);
    *shader = {};
    shader->file_header = 0x34333231u;
    shader->version = 0x18u;
    shader->header_size = sizeof(Shader);
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    recorder->RegisterShader(shader);
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    const auto groups = deltaGroupsHolding(reader, bytes);
    if (groups.empty()) return;
    std::string failure = "the page written before the capture, in a shader block the captured ranges cover only in part, is in memory delta groups";
    for (const auto held : groups) failure += " " + std::to_string(held);
    throw std::runtime_error(failure);
}

void testLateClear(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    auto* late = allocate(Size);
    std::fill_n(late, Size, std::byte{0});
    auto* x = late + 2 * FrameCaptureWriter::BlockBytes;
    auto* shader = reinterpret_cast<Shader*>(x + 2 * Page);
    *shader = {};
    shader->file_header = 0x34333231u;
    shader->version = 0x18u;
    shader->header_size = sizeof(Shader);
    auto* neighbor = x + FrameCaptureWriter::BlockBytes;
    std::fill_n(neighbor, Page, std::byte{0x5a});
    recorder->RegisterShader(shader);
    std::fill_n(neighbor, Page, std::byte{0});
    recorder->Suspend([] {});
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    std::uint64_t holding = 0;
    std::uint64_t lastGroup = 0;
    std::byte last{0};
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind != FrameMemoryKind::Delta || chunk.address > address(neighbor) || address(neighbor) + Page > chunk.address + chunk.bytes) continue;
        ++holding;
        if (chunk.group < lastGroup) continue;
        lastGroup = chunk.group;
        last = reader.Read(chunk)[address(neighbor) - chunk.address];
    }
    check(holding != 0, "the neighbour of the registered shader is in no memory delta");
    if (last != std::byte{0}) throw std::runtime_error("the last memory delta holding the neighbour of the registered shader (group " + std::to_string(lastGroup) + " of " + std::to_string(holding) + " holding it) has byte " + std::to_string(static_cast<int>(last)) + ", not the 0 the CPU stored after the registration");
}

std::byte lastDeltaByte(const FrameCaptureReader& reader, const std::byte* at) {
    std::uint64_t lastGroup = 0;
    std::byte last{0};
    bool held = false;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind != FrameMemoryKind::Delta || chunk.address > address(at) || address(at) >= chunk.address + chunk.bytes) continue;
        held = true;
        if (chunk.group < lastGroup) continue;
        lastGroup = chunk.group;
        last = reader.Read(chunk)[address(at) - chunk.address];
    }
    check(held, "the written byte is in no memory delta");
    return last;
}

void testStoreBetweenWalks(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0});
    auto* page = bytes + 2 * FrameCaptureWriter::BlockBytes;
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    std::fill_n(page, 16, std::byte{0x31});
    recorder->Suspend([] {});
    AgcDriver::GuestMemory::StoreOwnBytes(address(page), 16, [&] {
        std::fill_n(page, 16, std::byte{0x42});
        page[Page - 1] = std::byte{0x6b};
    });
    recorder->Suspend([] {});
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    const auto held = lastDeltaByte(reader, page + Page - 1);
    if (held != std::byte{0x6b}) throw std::runtime_error("the last memory delta holding the page has byte " + std::to_string(static_cast<int>(held)) + ", not the 107 the CPU stored between the walks of a driver store");
}

#ifdef _WIN32
bool consumeWrites(std::uintptr_t begin, std::size_t bytes, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
    std::vector<void*> pages(256);
    auto count = pages.size();
    if (!GuestArena::GuestArenaCollectWrites_nid_postfix(begin, bytes, pages.data(), &count, true)) return false;
    for (std::size_t index = 0; index < count; ++index) {
        const auto page = reinterpret_cast<std::uintptr_t>(pages[index]);
        written(context, page, page + 4096);
    }
    return true;
}

void testFreshNote(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0});
    auto* block = bytes + 2 * FrameCaptureWriter::BlockBytes;
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    recorder->Suspend([] {});
    struct FreshHookReset {
        ~FreshHookReset() { AgcDriver::GuestMemory::SetFreshCollectHook(nullptr); }
    } reset;
    AgcDriver::GuestMemory::SetFreshCollectHook(&consumeWrites);
    *(block + Page) = std::byte{0x6b};
    AgcDriver::GuestMemory::MarkWritten(address(block + 3 * Page), 16);
    AgcDriver::GuestMemory::SetFreshCollectHook(nullptr);
    recorder->Suspend([] {});
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    const auto held = lastDeltaByte(reader, block + Page);
    if (held != std::byte{0x6b}) throw std::runtime_error("the last memory delta holding the page has byte " + std::to_string(static_cast<int>(held)) + ", not the 107 the CPU stored before a fresh collect noted the page");
}
#endif

std::uintptr_t mockHandles = 1;
std::map<VkFence, bool> mockFences;

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer* commands) {
    *commands = reinterpret_cast<VkCommandBuffer>(mockHandles++);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockFreeCommandBuffers(VkDevice, VkCommandPool, std::uint32_t, const VkCommandBuffer*) {}

VKAPI_ATTR VkResult VKAPI_CALL mockCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) {
    *fence = reinterpret_cast<VkFence>(mockHandles++);
    mockFences[*fence] = false;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks*) {
    mockFences.erase(fence);
}

VKAPI_ATTR VkResult VKAPI_CALL mockGetFenceStatus(VkDevice, VkFence fence) {
    return mockFences.at(fence) ? VK_SUCCESS : VK_NOT_READY;
}

VKAPI_ATTR VkResult VKAPI_CALL mockWaitForFences(VkDevice, std::uint32_t count, const VkFence* fences, VkBool32, std::uint64_t) {
    for (std::uint32_t index = 0; index < count; ++index) mockFences.at(fences[index]) = true;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockResetFences(VkDevice, std::uint32_t count, const VkFence* fences) {
    for (std::uint32_t index = 0; index < count; ++index) mockFences.at(fences[index]) = false;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockEndCommandBuffer(VkCommandBuffer) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockQueueSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockDeviceWaitIdle(VkDevice) {
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockCmdUpdateBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, const void*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdBeginQuery(VkCommandBuffer, VkQueryPool, std::uint32_t, VkQueryControlFlags) {}
VKAPI_ATTR void VKAPI_CALL mockCmdEndQuery(VkCommandBuffer, VkQueryPool, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdResetQueryPool(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t) {}
VKAPI_ATTR void VKAPI_CALL mockCmdCopyQueryPoolResults(VkCommandBuffer, VkQueryPool, std::uint32_t, std::uint32_t, VkBuffer, VkDeviceSize, VkDeviceSize, VkQueryResultFlags) {}
VKAPI_ATTR void VKAPI_CALL mockCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline) {}
VKAPI_ATTR void VKAPI_CALL mockCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, std::uint32_t, std::uint32_t, const void*) {}
VKAPI_ATTR void VKAPI_CALL mockCmdDispatch(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t) {}

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateCommandBuffers)},
        {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(mockFreeCommandBuffers)},
        {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(mockCreateFence)},
        {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyFence)},
        {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(mockGetFenceStatus)},
        {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(mockWaitForFences)},
        {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(mockResetFences)},
        {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockBeginCommandBuffer)},
        {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockEndCommandBuffer)},
        {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(mockQueueSubmit)},
        {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(mockDeviceWaitIdle)},
        {"vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCmdUpdateBuffer)},
        {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(mockCmdPipelineBarrier)},
        {"vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBeginQuery)},
        {"vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(mockCmdEndQuery)},
        {"vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(mockCmdResetQueryPool)},
        {"vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(mockCmdCopyQueryPoolResults)},
        {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(mockCmdBindPipeline)},
        {"vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(mockCmdPushConstants)},
        {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(mockCmdDispatch)},
    };
    const auto found = table.find(name);
    return found == table.end() ? nullptr : found->second;
}

AgcDriver::Graphics::Context mockContext() {
    AgcDriver::Graphics::Context context{};
    context.device = reinterpret_cast<VkDevice>(mockHandles++);
    context.queue = reinterpret_cast<VkQueue>(mockHandles++);
    context.deviceProc = mockProc;
    return context;
}

constexpr std::size_t FilledBytes = 4096;
std::atomic<std::byte*> pendingFill{nullptr};
std::atomic<int> landedFills{0};

void landPendingFill(std::uint64_t begin, std::size_t bytes) {
    auto* const fill = pendingFill.load();
    if (fill == nullptr || begin >= address(fill) + FilledBytes || address(fill) >= begin + bytes) return;
    std::fill_n(fill, FilledBytes, std::byte{0x5a});
    pendingFill = nullptr;
    ++landedFills;
}

void testGpuWriteBeforeDelta(const std::filesystem::path& directory) {
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    std::fill_n(bytes, FrameCaptureWriter::BlockBytes, std::byte{0x11});
    auto* page = bytes + FilledBytes;
    const std::vector<std::byte> cleared(FilledBytes);
    {
        FrameCaptureWriter writer(directory);
        writer.WriteDelta(1, 0, address(page), cleared);
        writer.Finish("{}");
    }
    const FrameCaptureReader reader(directory);
    Options options;
    options.pace = false;
    options.release = false;
    Replay replay{options, reader};
    collectGroups(replay);
    struct ActiveRecorder {
        std::unique_ptr<AgcDriver::Graphics::Recorder> recorder;
        ~ActiveRecorder() {
            AgcDriver::GuestMemory::SetFlushHook(nullptr);
            pendingFill = nullptr;
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            recorder.reset();
        }
    } active{std::make_unique<AgcDriver::Graphics::Recorder>(mockContext())};
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        active.recorder->Activate();
        active.recorder->NotePendingWrite(address(page), FilledBytes);
    }
    AgcDriver::GuestMemory::SetFlushHook(&landPendingFill);
    pendingFill = page;
    check(AgcDriver::Graphics::Recorder::SnapshotWriteOverlaps(address(page), FilledBytes), "the GPU write is not pending in the driver's write snapshot");
    replay.execute(memoryCall(1, 1));
    std::vector<std::byte> replayed(FilledBytes);
    AgcDriver::GuestMemory::ReadCommitted(address(page), replayed);
    check(landedFills.load() == 1, "the GPU write pending over the page never landed, so the test did not exercise the replay's order");
    const auto stale = std::find_if(replayed.begin(), replayed.end(), [](std::byte value) { return value != std::byte{0}; });
    if (stale != replayed.end()) throw std::runtime_error("the replay restored a memory delta while a GPU write over its range was still pending, and the next read landed that write over it: the page holds byte " + std::to_string(static_cast<int>(*stale)) + ", not the 0 the capture recorded after the write");
}

void testVersionOneRefused(const std::filesystem::path& directory) {
    constexpr std::size_t Size = FrameCaptureWriter::BlockBytes;
    constexpr std::string_view VersionOne = "aps5-frame-calls 1\n";
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0x55});
    std::uintptr_t base = 0;
    std::size_t arenaSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &arenaSize);
    {
        FrameCaptureWriter writer(directory);
        writer.WriteRange(address(bytes), Size, FrameRangeKind::Watched, ReadWrite, FrameRangeAccess::Readable | FrameRangeAccess::Writable | FrameRangeAccess::Gpu);
        writer.WriteSnapshot(address(bytes), std::vector<std::byte>(Size, std::byte{0x7f}));
        writer.Finish("{\"arena_base\":" + std::to_string(base) + "}");
    }
    {
        std::fstream calls(directory / FrameCaptureWriter::CallsName, std::ios::binary | std::ios::in | std::ios::out);
        calls.write(VersionOne.data(), static_cast<std::streamsize>(VersionOne.size()));
        check(calls.good(), "cannot write the calls format 1 magic");
    }
    const FrameCaptureReader reader(directory);
    std::string refusal;
    try {
        restoreMemory(reader);
    } catch (const std::runtime_error& error) {
        refusal = error.what();
    }
    check(!refusal.empty(), "the replay restored a calls format 1 capture, which does not record whether GPU writes had landed before its memory deltas were read");
    if (refusal.find("record the capture again") == std::string::npos) throw std::runtime_error("the calls format 1 capture was refused for another reason: " + refusal);
    check(std::all_of(bytes, bytes + Size, [](std::byte value) { return value == std::byte{0x55}; }), "the replay restored memory before refusing the calls format 1 capture");
}

void testEopTokens() {
    Frames frames;
    auto output = std::make_shared<ReplayOutput>(frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    std::barrier together(2);
    std::atomic<int> wrong{0};
    const auto worker = [&](int lane) {
        for (int round = 0; round < 2000; ++round) {
            const auto requested = AgcDriver::EopFlipTokenBase + round * 8 + lane * 2;
            together.arrive_and_wait();
            try {
                if (AgcDriverReserveEopFlipAt_nid_postfix({7, -1, 1, 0}, requested) != requested) ++wrong;
            } catch (const std::exception&) { ++wrong; }
        }
    };
    std::thread first(worker, 0), second(worker, 1);
    first.join();
    second.join();
    check(wrong.load() == 0, "concurrent explicit EOP reservations returned the wrong token");
    const auto high = AgcDriver::EopFlipTokenBase + 20000;
    check(AgcDriverReserveEopFlipAt_nid_postfix({7, -1, 1, 0}, high) == high, "explicit high token was not reserved");
    check(AgcDriverReserveEopFlipAt_nid_postfix({7, -1, 1, 0}, high - 1) == high - 1, "explicit lower token was not reserved");
    check(AgcDriverReserveEopFlip_nid_postfix({7, -1, 1, 0}) == high + 1, "ordinary reservation reused an explicit token");
    bool rejected = false;
    try { AgcDriverReserveEopFlipAt_nid_postfix({7, -1, 1, 0}, high); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "duplicate explicit token was not rejected");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

class ReservationOutput final : public AgcDriver::IVideoOutput {
public:
    explicit ReservationOutput(Frames& frames) : frames(frames) {}
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override {
        ++reservations;
        if (failure) std::rethrow_exception(failure);
        return available ? std::make_shared<ReplayFlip>(frames) : nullptr;
    }
    void Fail(std::exception_ptr error) noexcept override { frames.Failed(error); }
    std::exception_ptr failure;
    bool available = true;
    std::size_t reservations = 0;

private:
    Frames& frames;
};

FrameCall reservationCall(std::uint32_t token) {
    FrameCall call;
    call.kind = FrameCallKind::ReserveEopFlip;
    call.values = {7, static_cast<std::uint64_t>(-1), 1, 99, token};
    return call;
}

template <typename TException>
void expectReservationFailure(Replay& replay, const FrameCall& call) {
    const auto kept = replay.keptReservations.load();
    bool failed = false;
    try { replay.runWindow(std::span(&call, 1)); }
    catch (const TException&) { failed = true; }
    check(failed, "the replay accepted a failed EOP reservation");
    check(replay.keptReservations.load() == kept, "a failed EOP reservation was counted as kept");
}

void testRepeatedReservations(const std::filesystem::path& directory, bool driverFailure) {
    FrameCaptureWriter writer(directory);
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.pace = false;
    options.release = false;
    Replay replay{options, reader};
    auto output = std::make_shared<ReservationOutput>(replay.frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    AgcDriverRegisterVideoOutput_nid_postfix(8, output);
    const auto call = reservationCall(AgcDriver::EopFlipTokenBase);
    replay.execute(call);
    expectReservationFailure<AgcDriver::EopFlipAlreadyReserved>(replay, call);
    replay.pass = 1;
    if (driverFailure) {
        AgcDriverReportFailure_nid_postfix(std::make_exception_ptr(std::runtime_error("reservation driver failure")));
        expectReservationFailure<std::runtime_error>(replay, call);
        bool failed = false;
        try { AgcDriverShutdown_nid_postfix(); }
        catch (const std::runtime_error& error) { failed = std::string_view(error.what()) == "reservation driver failure"; }
        check(failed, "the injected driver failure was not propagated at shutdown");
        return;
    }
    replay.runWindow(std::span(&call, 1));
    check(replay.keptReservations.load() == 1 && output->reservations == 1, "a compatible pending reservation was not kept without allocating again");
    for (std::size_t field = 0; field < 4; ++field) {
        auto different = call;
        ++different.values[field];
        expectReservationFailure<std::runtime_error>(replay, different);
    }
    auto missing = call;
    missing.values[0] = 9;
    expectReservationFailure<std::runtime_error>(replay, missing);
    expectReservationFailure<std::runtime_error>(replay, reservationCall(AgcDriver::EopFlipTokenBase - 1));
    expectReservationFailure<std::runtime_error>(replay, reservationCall(AgcDriver::EopFlipTokenLimit + 1));
    const auto fresh = reservationCall(AgcDriver::EopFlipTokenBase + 1);
    output->failure = std::make_exception_ptr(std::bad_alloc{});
    expectReservationFailure<std::bad_alloc>(replay, fresh);
    output->failure = std::make_exception_ptr(std::runtime_error("reservation output failure"));
    expectReservationFailure<std::runtime_error>(replay, fresh);
    output->failure = nullptr;
    output->available = false;
    expectReservationFailure<std::runtime_error>(replay, fresh);
    output->available = true;
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
    auto replacement = std::make_shared<ReservationOutput>(replay.frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, replacement);
    replay.runWindow(std::span(&call, 1));
    check(replay.keptReservations.load() == 1 && replacement->reservations == 1, "a closed output's token was kept after reopening");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, replacement);
    AgcDriverUnregisterVideoOutput_nid_postfix(8, output);
}

class WaitingOutput final : public AgcDriver::IVideoOutput {
public:
    WaitingOutput(Frames& frames, const std::uint32_t* label) : frames(frames), label(label) {}
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo&) override {
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        while (*static_cast<const volatile std::uint32_t*>(label) == 0) {
            check(Clock::now() < deadline, "a blocking driver call prevented the later memory update");
            std::this_thread::yield();
        }
        return std::make_shared<ReplayFlip>(frames);
    }
    void Fail(std::exception_ptr error) noexcept override { frames.Failed(error); }

private:
    Frames& frames;
    const std::uint32_t* label;
};

void testTraceWindow(const std::filesystem::path& directory) {
    const char* traceDirectory = std::getenv("APS5_TRACE_DIR");
    check(traceDirectory != nullptr && AgcDriver::PerformanceTrace::Configured(), "the trace window test runs without APS5_TRACE_DIR and APS5_TRACE_FRAMES");
    const auto summaryPath = std::filesystem::path(traceDirectory) / "aps5-summary.jsonl";
    std::filesystem::remove(summaryPath);
    FrameCaptureWriter writer(directory);
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.pace = false;
    options.release = false;
    Replay replay{options, reader};
    auto output = std::make_shared<ReplayOutput>(replay.frames);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    std::array<std::uint32_t, AgcDriver::FlipPacketWords> words{AgcDriver::FlipPacketHeader, 7, 0xffffffffu, 1, 0, 0};
    FrameCall flip;
    flip.kind = FrameCallKind::Submit;
    flip.thread = 1;
    flip.address = address(words.data());
    flip.size = words.size();
    const std::array calls{flip, flip, flip};
    replay.runWindow(calls);
    AgcDriverWaitIdle_nid_postfix();
    check(replay.frames.Times().size() == calls.size(), "a replayed flip was lost");
    AgcDriver::PerformanceTrace::StopOutput();
    std::ifstream summary(summaryPath);
    std::string line;
    check(static_cast<bool>(std::getline(summary, line)), "replayed frames did not open and close the trace window");
    check(line.find("\"presents\":2,") != std::string::npos, "the trace window did not count each replayed flip as a present");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

void testBlockingSubmission(const std::filesystem::path& directory) {
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    auto* label = reinterpret_cast<std::uint32_t*>(bytes);
    const std::uint32_t ready = 1;
    FrameCaptureWriter writer(directory);
    writer.WriteDelta(1, 0, address(label), std::as_bytes(std::span(&ready, 1)));
    writer.Finish("{}");
    const FrameCaptureReader reader(directory);
    Options options;
    options.pace = false;
    options.release = false;
    Replay replay{options, reader};
    collectGroups(replay);
    auto output = std::make_shared<WaitingOutput>(replay.frames, label);
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    std::array<std::uint32_t, AgcDriver::FlipPacketWords> words{AgcDriver::FlipPacketHeader, 7, 0xffffffffu, 1, 0, 0};
    for (const auto kind : {FrameCallKind::Submit, FrameCallKind::Flip, FrameCallKind::ReserveEopFlip}) {
        *label = 0;
        FrameCall submit;
        submit.kind = kind;
        submit.thread = 1;
        submit.address = address(words.data());
        submit.size = words.size();
        submit.values = {7, static_cast<std::uint64_t>(-1), 1, 0, AgcDriver::EopFlipTokenBase};
        const std::array calls{submit, memoryCall(1, 2)};
        replay.runWindow(calls);
        check(*label == ready && replay.unfinished == 0, "blocking submission did not complete");
    }
    check(replay.frames.Times().size() == 2, "blocking submissions lost a flip");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

void testShaderCode(const std::filesystem::path& directory, bool host, std::string_view mode = {}) {
    const bool prologue = mode.starts_with("shader_prologue_");
    const bool readOnly = mode.ends_with("readonly");
    const bool hostHeader = readOnly || mode.ends_with("header");
    std::byte* hostMemory = nullptr;
    auto* bytes = allocate(1u << 20);
    auto* shader = reinterpret_cast<Shader*>(bytes);
    auto* code = bytes + (1u << 19) + 256;
    constexpr std::size_t CodeBytes = 3 * FrameCaptureWriter::BlockBytes;
#if defined(_WIN32) || defined(__linux__)
    if (host) {
#ifdef _WIN32
        auto* memory = static_cast<std::byte*>(VirtualAlloc(nullptr, CodeBytes + 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        void* mapped = mmap(nullptr, CodeBytes + 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        auto* memory = mapped != MAP_FAILED ? static_cast<std::byte*>(mapped) : nullptr;
#endif
        check(memory != nullptr, "host shader allocation failed");
        if (!prologue) {
            GuestAllocations::Mutation mutation;
            mutation.Add(memory, CodeBytes + 4096, true, true, true);
        }
        hostMemory = memory;
        if (hostHeader) shader = reinterpret_cast<Shader*>(memory);
        code = memory + 256;
    }
#endif
    *shader = {};
    shader->file_header = 0x34333231u;
    shader->version = 0x18u;
    shader->header_size = sizeof(Shader);
    shader->shader_size = CodeBytes;
    shader->code = code;
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    if (!prologue) recorder->FrameBoundary();
    std::vector<std::byte> expected(CodeBytes);
    for (std::size_t i = 0; i < expected.size(); ++i) expected[i] = static_cast<std::byte>(i * 31 + 17);
    std::memcpy(code, expected.data(), expected.size());
    std::vector<std::byte> expectedHeader(sizeof(Shader));
    std::memcpy(expectedHeader.data(), shader, expectedHeader.size());
    if (readOnly) {
#ifdef _WIN32
        DWORD previous = 0;
        check(VirtualProtect(hostMemory, CodeBytes + 4096, PAGE_READONLY, &previous) != 0, "cannot protect host shader pages");
#elif defined(__linux__)
        check(mprotect(hostMemory, CodeBytes + 4096, PROT_READ) == 0, "cannot protect host shader pages");
#endif
    }
    recorder->RegisterShader(shader);
    recorder->FrameBoundary();
    if (prologue) recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    std::vector<std::byte> captured(CodeBytes);
    std::vector<std::byte> capturedHeader(sizeof(Shader));
    const auto applyBytes = [&](const FrameMemoryChunk& chunk, std::uint64_t target, std::span<std::byte> bytes) {
        const auto begin = std::max(target, chunk.address);
        const auto end = std::min(target + bytes.size(), chunk.address + chunk.bytes);
        if (begin >= end) return;
        const auto data = reader.Read(chunk);
        std::memcpy(bytes.data() + begin - target, data.data() + begin - chunk.address, end - begin);
    };
    const auto apply = [&](const FrameMemoryChunk& chunk) {
        applyBytes(chunk, address(code), captured);
        if (hostHeader) applyBytes(chunk, address(shader), capturedHeader);
    };
    for (const auto& chunk : reader.Chunks()) if (chunk.kind == FrameMemoryKind::Snapshot) apply(chunk);
    bool registered = false;
    for (const auto& call : reader.Calls()) {
        if (call.kind == FrameCallKind::RegisterShader) {
            registered = true;
            break;
        }
        if (call.kind != FrameCallKind::Memory) continue;
        for (const auto& chunk : reader.Chunks()) {
            if (chunk.kind == FrameMemoryKind::Delta && chunk.group == call.values.at(0)) apply(chunk);
        }
    }
    if (hostHeader && captured == expected) check(capturedHeader == expectedHeader, "the host shader header was not captured before registration");
    if (readOnly && captured == expected) {
        const bool protectedHeader = std::ranges::any_of(reader.Chunks(), [&](const auto& chunk) {
            return chunk.kind == FrameMemoryKind::Range && chunk.address <= address(shader) &&
                address(shader) - chunk.address < chunk.bytes && (chunk.access & AgcDriver::FrameRangeAccess::Writable) == 0;
        });
        check(protectedHeader, "the read-only host shader was recorded with write access");
    }
    check(registered && captured == expected, "the complete separate shader code was not captured before registration");
    CapturedMemory memory(reader);
    std::map<std::uint64_t, std::vector<const FrameMemoryChunk*>> groups;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind == FrameMemoryKind::Delta) groups[chunk.group].push_back(&chunk);
    }
    std::optional<std::vector<std::byte>> listed;
    for (const auto& call : reader.Calls()) {
        if (call.kind == FrameCallKind::RegisterShader) {
            listed = memory.Read(address(code), CodeBytes);
            break;
        }
        if (call.kind != FrameCallKind::Memory) continue;
        if (const auto found = groups.find(call.values.at(0)); found != groups.end()) memory.Apply(found->second);
    }
    check(listed && *listed == expected, "the offline shader listing read other code than the capture holds at the registration");
}

void writeInspectionCapture(const std::filesystem::path& directory) {
    constexpr std::uint64_t Commands = 0x1000000001ull;
    constexpr std::uint64_t Header = 0x2000000000ull;
    constexpr std::uint64_t Code = 0x2000010000ull;
    const std::array<std::uint32_t, 27> commands{
        0x00010000u, 0xc0053c00u, 0xc0064900u, 0x80000000u, 0xffff1000u,
        0xc0053c00u, 0x13u, 0x12345678u, 0x9abcdef0u, 0x10203040u, 0xf0f0f0f0u, 4u,
        0xc0064900u, 1u, 2u, 3u, 4u, 5u, 6u, 7u,
        0xc0033700u, 0x500u, 0xdeadbeefu, 0x12u, 0x1337u,
        0x00030000u, 0xc0053c00u};
    std::array<std::uint8_t, 16> code{0, 1, 2, 3, 4, 5, 6, 7, 255, 128, 13, 10, 39, 36, 38, 0};
    Shader shader{};
    shader.code = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(Code));
    shader.shader_size = code.size();
    FrameCaptureWriter writer(directory);
    writer.WriteRange(Commands, 6, FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable);
    writer.WriteRange(Commands + 6, sizeof(commands) - 6, FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable);
    writer.WriteRange(Header, sizeof(shader), FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable);
    writer.WriteRange(Code, code.size(), FrameRangeKind::Host, ReadWrite, FrameRangeAccess::Readable);
    writer.WriteSnapshot(Header, std::as_bytes(std::span(&shader, 1)));
    writer.WriteSnapshot(Code, std::as_bytes(std::span(code)));
    writer.WriteDelta(1, 0, Commands, std::as_bytes(std::span(commands)));
    const std::uint32_t reference = 0x55667788u;
    writer.WriteDelta(2, 0, Commands + 9 * sizeof(std::uint32_t), std::as_bytes(std::span(&reference, 1)));
    code[0] = 0x42;
    writer.WriteDelta(3, 0, Code, std::as_bytes(std::span(code)));
    FrameCall call;
    const auto write = [&](FrameCallKind kind, std::uint64_t location, std::uint64_t size, std::vector<std::uint64_t> values = {}) {
        call.kind = kind;
        ++call.ordinal;
        call.address = location;
        call.size = size;
        call.values = std::move(values);
        call.thread = 7;
        call.queue = 0x42;
        call.accepted = 12;
        call.completed = 9;
        writer.WriteCall(call);
    };
    write(FrameCallKind::Submit, Commands, commands.size());
    write(FrameCallKind::Memory, 0, 0, {1});
    write(FrameCallKind::RegisterShader, Header, 0);
    write(FrameCallKind::FrameStart, 0, 0);
    write(FrameCallKind::Submit, Commands, commands.size());
    write(FrameCallKind::Memory, 0, 0, {2});
    write(FrameCallKind::Submit, Commands, commands.size());
    write(FrameCallKind::RegisterShader, Header, 0);
    write(FrameCallKind::Memory, 0, 0, {3});
    write(FrameCallKind::RegisterShader, Header, 0);
    write(FrameCallKind::Flip, 0, 0);
    write(FrameCallKind::ReserveEopFlip, 0, 0);
    write(FrameCallKind::Submit, 0x3000000000ull, 1);
    write(FrameCallKind::Submit, Commands, std::numeric_limits<std::uint64_t>::max());
    write(FrameCallKind::Submit, std::numeric_limits<std::uint64_t>::max() - 1, 1);
    write(FrameCallKind::Submit, Commands, 0);
    writer.Finish("{}");
}

void testColdPipelines() {
    const auto* inherited = std::getenv("APS5_HEADLESS_PIPELINE_CACHE");
    check(inherited != nullptr && std::string_view(inherited) == "1", "pipeline cache setting was not inherited");
    Options options;
    options.coldPipelines = true;
    configureEnvironment(options);
    check(std::getenv("APS5_HEADLESS_PIPELINE_CACHE") == nullptr, "cold replay kept the inherited pipeline cache setting");
    check(std::getenv("APS5_FRAME_CAPTURE") == nullptr, "replay kept the capture setting");
    options.coldPipelines = false;
    configureEnvironment(options);
    const auto* warm = std::getenv("APS5_HEADLESS_PIPELINE_CACHE");
    check(warm != nullptr && std::string_view(warm) == "1", "warm replay did not enable the pipeline cache");
#ifdef _WIN32
    _putenv_s("APS5_HEADLESS_PIPELINE_CACHE", "0");
#else
    setenv("APS5_HEADLESS_PIPELINE_CACHE", "0", 1);
#endif
    options.coldPipelines = true;
    configureEnvironment(options);
    check(std::getenv("APS5_HEADLESS_PIPELINE_CACHE") == nullptr, "cold replay kept a present zero pipeline cache setting");
}

std::optional<std::pair<std::uint64_t, std::byte>> lastRecorded(const FrameCaptureReader& reader, const std::byte* at) {
    std::optional<std::pair<std::uint64_t, std::byte>> last;
    for (const auto& chunk : reader.Chunks()) {
        if ((chunk.kind != FrameMemoryKind::Delta && chunk.kind != FrameMemoryKind::Snapshot) || chunk.address > address(at) || address(at) >= chunk.address + chunk.bytes) continue;
        const std::uint64_t group = chunk.kind == FrameMemoryKind::Delta ? chunk.group : 0;
        if (last && group < last->first) continue;
        last = std::pair{group, reader.Read(chunk)[address(at) - chunk.address]};
    }
    return last;
}

std::string describeRecorded(const std::optional<std::pair<std::uint64_t, std::byte>>& last) {
    if (!last) return "in no memory delta and not in the snapshot";
    const auto where = last->first == 0 ? std::string("last recorded by the snapshot") : "last recorded in memory delta group " + std::to_string(last->first);
    return where + " with byte " + std::to_string(static_cast<int>(last->second));
}

void testStoreWindow(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(Size);
    std::fill_n(bytes, Size, std::byte{0});
    auto* page = bytes + 2 * FrameCaptureWriter::BlockBytes;
    auto* label = reinterpret_cast<std::uint32_t*>(page + 0x100);
    auto* title = page + 0x800;
    auto* own = bytes + 4 * FrameCaptureWriter::BlockBytes;
    own[4 * Page + 0x123] = std::byte{0x6d};
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    AgcDriver::GuestMemory::StoreOwnBytes(address(label), sizeof(*label), [&] {
        *label = 1;
        std::thread([&] { std::fill_n(title, 64, std::byte{0x7e}); }).join();
    });
    const std::array<std::byte, 8> eight{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}};
    AgcDriver::GuestMemory::StoreOwnBytes(address(own + Page + 0x40), eight);
    AgcDriver::GuestMemory::Write(address(own + 2 * Page + 0x80), eight, 4);
    AgcDriverStoreGuestLabel_nid_postfix(reinterpret_cast<std::uint64_t*>(own + 3 * Page + 0x10), 7);
    const std::vector<std::byte> original(Page, std::byte{0});
    auto current = original;
    current[0x10] = std::byte{0x5c};
    AgcDriver::GuestMemory::WriteChanged(address(own + 4 * Page), current, original);
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    check(reader.Header().find("stopped") == std::string::npos, ("the capture stopped: " + reader.Header()).c_str());
    std::string failures;
    const auto last = lastRecorded(reader, title);
    if (!last || last->first == 0 || last->second != std::byte{0x7e}) failures = "the store another thread made in the page of a driver store during the store is " + describeRecorded(last) + ", not 126";
    const std::array<const char*, 4> stores{"StoreOwnBytes with given bytes", "Write", "AgcDriverStoreGuestLabel", "WriteChanged"};
    for (std::size_t index = 0; index < stores.size(); ++index) {
        const auto groups = deltaGroupsHolding(reader, own + (index + 1) * Page);
        if (groups.empty()) continue;
        failures += std::string(failures.empty() ? "" : "; ") + "the page " + stores[index] + " stored into with no other store in it is in memory delta groups";
        for (const auto held : groups) failures += " " + std::to_string(held);
    }
    auto* after = bytes + 6 * FrameCaptureWriter::BlockBytes;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> written;
    const auto since = AgcDriver::GuestMemory::CollectWrittenBlocks(address(after), Page, std::numeric_limits<std::uint64_t>::max(), written);
    AgcDriver::GuestMemory::StoreOwnBytes(address(after + 0x100), 8, [&] { after[0x100] = std::byte{1}; });
    AgcDriver::GuestMemory::CollectWrittenBlocks(address(after), Page, since, written);
    if (!written.empty()) failures += std::string(failures.empty() ? "" : "; ") + "a driver store made after the capture finished was listed as a CPU write";
    if (!failures.empty()) throw std::runtime_error(failures);
}

void testStoreWindowUnwritten(const std::filesystem::path& directory) {
    constexpr std::size_t Size = 1u << 20;
    auto* bytes = allocate(Size);
    auto* page = bytes + 2 * FrameCaptureWriter::BlockBytes;
    const std::array<std::byte, 16> zero{};
    AgcDriver::GuestMemory::Write(address(page + 0x100), zero);
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    recorder->Suspend([] {});
    auto* label = reinterpret_cast<std::uint32_t*>(page + 0x100);
    auto* title = page + 0x800;
    AgcDriver::GuestMemory::StoreOwnBytes(address(label), sizeof(*label), [&] {
        *label = 1;
        std::thread([&] { std::fill_n(title, 64, std::byte{0x7e}); }).join();
    });
    recorder->Suspend([] {});
    recorder->Suspend([] {});
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    check(reader.Header().find("stopped") == std::string::npos, ("the capture stopped: " + reader.Header()).c_str());
    const auto last = lastRecorded(reader, title);
    if (!last || last->first == 0 || last->second != std::byte{0x7e}) throw std::runtime_error("the store another thread made during a driver store into a page the CPU has not written is " + describeRecorded(last) + ", not 126");
}

void testStoreWindowStopped(const std::filesystem::path& directory) {
    constexpr std::size_t Page = 4096;
    auto* bytes = allocate(FrameCaptureWriter::BlockBytes);
    std::fill_n(bytes, FrameCaptureWriter::BlockBytes, std::byte{0});
    auto* recorder = AgcDriver::FrameRecorder::Active();
    check(recorder != nullptr, "frame recorder is disabled");
    recorder->FrameBoundary();
    const FrameCaptureReader reader(directory);
    check(reader.Header().find("stopped: less than 4 GiB free on the capture volume") != std::string::npos, "the capture did not stop when it started");
    std::vector<std::pair<std::uint64_t, std::uint64_t>> written;
    const auto since = AgcDriver::GuestMemory::CollectWrittenBlocks(address(bytes), Page, std::numeric_limits<std::uint64_t>::max(), written);
    AgcDriver::GuestMemory::StoreOwnBytes(address(bytes + 0x100), 8, [&] { bytes[0x100] = std::byte{1}; });
    AgcDriver::GuestMemory::CollectWrittenBlocks(address(bytes), Page, since, written);
    if (!written.empty()) throw std::runtime_error("a driver store made after the capture stopped was listed as a CPU write");
}

}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "expected a test name and capture directory");
        const std::string mode = argv[1];
        const std::filesystem::path directory = argv[2];
        if (mode == "inspect_capture") {
            writeInspectionCapture(directory);
            return 0;
        }
        if (mode == "native_repeat_htile_orphan") testNativeRepeatHtileOrphan(directory);
        else if (mode == "native_repeat_pipelines") testNativeRepeatPipelines(directory);
        else if (mode == "native_repeat_failure") testNativeRepeatFailure();
        else if (mode == "native_repeat_preflight") testNativeRepeatPreflight(directory);
        else if (mode == "native_repeat_stored_owner") testNativeRetiredStoredOwner(directory);
        else if (mode == "native_repeat_state" || mode == "native_repeat_clean" || mode == "native_repeat_companions") testNativeRepeat(directory, mode);
        else if (mode == "order") testMemoryOrder(directory);
        else if (mode == "zero") testReusedMemory(directory);
        else if (mode.starts_with("shader_prologue_")) testShaderCode(directory, true, mode);
        else if (mode == "shared_page") testSharedPage(directory);
        else if (mode == "watched_existing") testWatchedExisting(directory);
        else if (mode == "sub_page_resets") testSubPageResets(directory);
        else if (mode == "partial_walk") testPartialWalk(directory);
        else if (mode == "large_range") testLargeRange(directory);
        else if (mode == "flushed_owner") {
            if (!testFlushedOwner(directory)) return 77;
        }
        else if (mode == "untouched_gpu_owner") {
            if (!testUntouchedGpuOwner(directory)) return 77;
        }
        else if (mode == "host_neighbors") testHostNeighbors(directory);
        else if (mode == "unwatched") testUnwatched();
        else if (mode == "once") testPagesCollectedOnce(directory);
        else if (mode == "pending_result") testPendingResultRead(directory);
        else if (mode == "partly_covered") testPartlyCoveredBlock(directory);
        else if (mode == "late_clear") testLateClear(directory);
        else if (mode == "store_between_walks") testStoreBetweenWalks(directory);
        else if (mode == "store_window") testStoreWindow(directory);
        else if (mode == "store_window_unwritten") testStoreWindowUnwritten(directory);
        else if (mode == "store_window_stopped") testStoreWindowStopped(directory);
#ifdef _WIN32
        else if (mode == "fresh_note") testFreshNote(directory);
#endif
        else if (mode == "gpu_write_before_delta") testGpuWriteBeforeDelta(directory);
        else if (mode == "calls_v1") testVersionOneRefused(directory);
        else if (mode == "trace_window") testTraceWindow(directory);
        else if (mode == "tokens") testEopTokens();
        else if (mode == "reservations" || mode == "reservation_failure") testRepeatedReservations(directory, mode == "reservation_failure");
        else if (mode == "blocking") testBlockingSubmission(directory);
        else if (mode == "environment") testColdPipelines();
        else if (mode == "shader" || mode == "shader_host") testShaderCode(directory, mode == "shader_host");
        else throw std::runtime_error("unknown test");
        if (mode != "reservation_failure") AgcDriverShutdown_nid_postfix();
        std::puts("frame replay state test passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
