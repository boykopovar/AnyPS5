#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "RuntimeAbi.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
namespace Abi = ShaderRecompiler::RuntimeAbi;

constexpr int VulkanTestSkipped = 77;

void require(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error(what);
}

alignas(Texture) std::array<std::array<std::byte, sizeof(Texture)>, 8> fakeTextures{};

std::shared_ptr<Texture> fakeTexture(std::size_t index) {
    return std::shared_ptr<Texture>(reinterpret_cast<Texture*>(fakeTextures[index].data()), [](Texture*) {});
}

void textureSlots(const Context& context, VkImageView view) {
    BindlessHeap heap(context);
    const auto kind = Abi::FirstImageBinding;
    auto first = fakeTexture(0);
    const auto slot = heap.TextureSlot(kind, first, view, VK_IMAGE_LAYOUT_GENERAL);
    require(slot.has_value(), "the first texture takes a slot");
    require(heap.SlotTexture(kind, *slot) == first.get(), "the slot holds the first texture");
    const auto generation = heap.SlotGeneration(kind, *slot);

    auto second = fakeTexture(1);
    require(!heap.TextureSlot(kind, second, view, VK_IMAGE_LAYOUT_GENERAL).has_value(), "a held slot is not reused");
    first.reset();
    const auto reused = heap.TextureSlot(kind, second, view, VK_IMAGE_LAYOUT_GENERAL);
    require(reused.has_value() && *reused == *slot, "the released texture's slot is reused");
    require(heap.SlotGeneration(kind, *slot) != generation, "a freed slot moves its generation");
    require(heap.SlotTexture(kind, *slot) == second.get(), "the reused slot holds the second texture");
    const auto counts = heap.Statistics();
    require(counts.slotsFreed == 1u && counts.slotsReused == 1u, "one slot freed and reused");

    auto shared = fakeTexture(2);
    const auto otherKind = kind + 1u;
    const auto firstShared = heap.TextureSlot(kind, shared, view, VK_IMAGE_LAYOUT_GENERAL);
    require(!firstShared.has_value(), "the capped kind has no slot left for a third texture");
    const auto sharedSlot = heap.TextureSlot(otherKind, shared, view, VK_IMAGE_LAYOUT_GENERAL);
    require(sharedSlot.has_value() && shared->HeapHolds() == 1u, "a texture placed by the heap counts one heap hold");
    second.reset();
    shared.reset();
    const auto freedBefore = heap.Statistics().slotsFreed;
    auto third = fakeTexture(0);
    require(heap.TextureSlot(kind, third, view, VK_IMAGE_LAYOUT_GENERAL).has_value(), "a texture held only by the heap is swept");
    require(heap.Statistics().slotsFreed == freedBefore + 2u, "one sweep frees every slot whose texture only the heap holds");
}

void samplerSlots(const Context& context) {
    BindlessHeap heap(context);
    auto cache = std::make_unique<SamplerCache>();
    const std::array<std::uint32_t, 4> firstWords{0u, 0u, 0u, 0u};
    const std::array<std::uint32_t, 4> secondWords{0u, 0u, 0x00100000u, 0u};
    auto first = cache->Get(context, firstWords, false);
    const auto slot = heap.SamplerSlot(first);
    require(slot.has_value(), "the first sampler takes a slot");
    const auto generation = heap.SamplerGeneration(*slot);
    require(first->CacheKey() != nullptr, "a cached sampler carries its key");
    first.reset();
    cache.reset();
    cache = std::make_unique<SamplerCache>();
    auto second = cache->Get(context, secondWords, false);
    const auto reused = heap.SamplerSlot(second);
    require(reused.has_value() && *reused == *slot, "the released sampler's slot is reused");
    require(heap.SamplerGeneration(*slot) != generation, "a freed sampler slot moves its generation");
}

void viewSlots(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    const auto kind = Abi::FirstStorageImageBinding + 27u;
    const int first = 0, second = 0;
    const auto slot = heap.ViewSlot(kind, &first, view);
    require(slot.has_value() && heap.ViewSlot(kind, &first, view) == slot, "a view keeps its slot");
    require(!heap.ViewSlot(kind, &second, view).has_value(), "a full kind refuses another owner's view");
    const auto generation = heap.SlotGeneration(kind, *slot);
    heap.ReleaseViewSlots(&first);
    require(heap.SlotGeneration(kind, *slot) != generation, "a released view slot moves its generation");
    require(heap.ViewSlot(kind, &second, view) == slot, "a released view slot is reused");
    heap.ReleaseViewSlots(&second);
}

void sweptOwnerReentry(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    const auto kind = Abi::FirstStorageImageBinding + 1u;
    int released = 0;
    const auto owner = [&] {
        return std::shared_ptr<void>(new int(0), [&context, &released](void* pointer) {
            if (auto* existing = BindlessHeap::Existing(context)) existing->ReleaseViewSlots(pointer);
            ++released;
            delete static_cast<int*>(pointer);
        });
    };
    require(heap.StorageSlot(kind, owner(), view).has_value(), "the first storage owner takes the kind's only slot");
    require(heap.StorageSlot(kind, owner(), view).has_value() && released == 1,
            "a full kind sweeps an owner whose release calls back into the heap");
    BindlessHeap::NoteSweepDue();
    BindlessHeap::SweepIfDue();
    require(released == 2, "a due sweep releases an owner that calls back into the heap");
}

void ringSpans(const Context& context) {
    BindlessHeap heap(context);
    BindlessHeap::SpanMark unwritten;
    require(!heap.Held(unwritten), "a span never written is not held");
    BindlessHeap::SpanMark first;
    const auto span = heap.Allocate(16u, first);
    require(first.offset == span.offset && first.words == 16u && heap.Held(first), "a span written in the open chunk is held");
    constexpr std::size_t chunkWords = (4u << 20u) / sizeof(std::uint32_t);
    BindlessHeap::SpanMark filler;
    static_cast<void>(heap.Allocate(chunkWords - 1024u, filler));
    BindlessHeap::SpanMark crossing;
    static_cast<void>(heap.Allocate(4096u, crossing));
    require(!heap.Held(crossing), "a span over two chunks is never kept");
    require(heap.Held(first), "a span stays held while its batch keeps the chunk open");
    const auto counts = heap.Statistics();
    require(counts.spansWritten == 3u && counts.spansKept == 2u, "three spans written and two kept");
    BindlessHeap::SpanMark third;
    static_cast<void>(heap.Allocate(chunkWords, third));
    Recorder recorder(context);
    heap.Submitted(recorder, 0u);
    require(heap.Held(first), "a span stays held after its batch is submitted while the ring has not come back to its chunk");
    BindlessHeap::SpanMark fill;
    const auto untilWrap = [&] {
        const auto wraps = heap.Statistics().ringWraps;
        BindlessHeap::DataSpan last{};
        while (heap.Statistics().ringWraps == wraps) last = heap.Allocate(chunkWords, fill);
        return last.offset;
    };
    require(untilWrap() == (4u << 20u), "the ring skips a chunk a later batch still reads kept spans from");
    require(heap.Held(first), "the skipped chunk's kept span is still held");
    require(heap.Statistics().keptChunksSkipped == 1u, "one kept chunk skipped");
    heap.Submitted(recorder, 0u);
    require(untilWrap() == 0u && !heap.Held(first) && heap.Statistics().spansExpired == 1u, "a span the ring wrote over has expired");
}

}

void heapComesBack(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    const auto kind = Abi::FirstImageBinding;
    std::vector<std::shared_ptr<Texture>> held;
    for (std::size_t index = 0; index < 7u; ++index) {
        held.push_back(fakeTexture(index));
        require(heap.TextureSlot(kind, held.back(), view, VK_IMAGE_LAYOUT_GENERAL).has_value(),
                "a texture takes one of the eight capped slots");
    }
    require(BindlessHeap::NearlyFull(), "one free slot of eight refuses the heap");
    BindlessHeap::SweepIfDue();
    require(BindlessHeap::NearlyFull(), "a sweep that is not due leaves the heap refused");
    BindlessHeap::NoteSweepDue();
    BindlessHeap::SweepIfDue();
    require(BindlessHeap::NearlyFull(), "a sweep that frees nothing leaves the heap refused");
    held.clear();
    BindlessHeap::NoteSweepDue();
    BindlessHeap::SweepIfDue();
    require(!BindlessHeap::NearlyFull(), "a sweep that frees the slots of textures only the heap held takes the heap again");
    BindlessHeap::Release(context.device);
}

int main(int argc, char** argv) {
    const bool comesBack = argc > 1 && std::string(argv[1]) == "comes-back";
    try {
#ifdef _WIN32
        _putenv_s("APS5_GPU_HEAP", "1");
#else
        setenv("APS5_GPU_HEAP", "1", 1);
#endif
        std::unique_ptr<AgcDriver::VulkanDevice> device;
        try {
            device = std::make_unique<AgcDriver::VulkanDevice>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        if (!device->GraphicsContext().descriptorHeap) {
            std::printf("skipped, the device has no VK_EXT_descriptor_heap\n");
            return VulkanTestSkipped;
        }
#ifdef _WIN32
        _putenv_s("APS5_HEAP_SLOT_CAP", comesBack ? "8" : "1");
#else
        setenv("APS5_HEAP_SLOT_CAP", comesBack ? "8" : "1", 1);
#endif
        if (comesBack) {
            {
                const auto context = device->GraphicsContext();
                const PlaceholderImages placeholders(context);
                heapComesBack(context, placeholders.View(Abi::FirstImageBinding));
                device->WaitIdle();
            }
            device.reset();
            std::cout << "the refused heap comes back after a sweep\n";
            return 0;
        }
        {
            const auto context = device->GraphicsContext();
            const PlaceholderImages placeholders(context);
            textureSlots(context, placeholders.View(Abi::FirstImageBinding));
            samplerSlots(context);
            ringSpans(context);
            viewSlots(context, placeholders.View(Abi::FirstStorageImageBinding));
            sweptOwnerReentry(context, placeholders.View(Abi::FirstStorageImageBinding));
            device->WaitIdle();
        }
        device.reset();
        std::cout << "bindless heap slot reuse tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "bindless heap slot reuse tests failed: " << error.what() << '\n';
        return 1;
    }
}
