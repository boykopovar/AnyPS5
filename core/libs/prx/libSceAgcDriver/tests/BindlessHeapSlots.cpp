#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DescriptorHeapObjects.hpp"
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
alignas(StorageTexture) std::array<std::array<std::byte, sizeof(StorageTexture)>, 3> fakeImages{};

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
    const auto generation = heap.SlotGeneration(*slot);

    auto second = fakeTexture(1);
    require(!heap.TextureSlot(kind, second, view, VK_IMAGE_LAYOUT_GENERAL).has_value(), "a held slot is not reused");
    first.reset();
    const auto reused = heap.TextureSlot(kind, second, view, VK_IMAGE_LAYOUT_GENERAL);
    require(reused.has_value() && *reused == *slot, "the released texture's slot is reused");
    require(heap.SlotGeneration(*slot) != generation, "a freed slot moves its generation");
    require(heap.SlotTexture(kind, *slot) == second.get(), "the reused slot holds the second texture");
    const auto counts = heap.Statistics();
    require(counts.slotsFreed == 1u && counts.slotsReused == 1u, "one slot freed and reused");

    auto shared = fakeTexture(2);
    const auto otherKind = kind + 1u;
    require(!heap.TextureSlot(otherKind, shared, view, VK_IMAGE_LAYOUT_GENERAL).has_value(),
            "another kind finds the capped heap's only slot held");
    second.reset();
    const auto sharedSlot = heap.TextureSlot(otherKind, shared, view, VK_IMAGE_LAYOUT_GENERAL);
    require(sharedSlot.has_value() && *sharedSlot == *slot, "another kind takes the slot the first kind's texture left");
    require(shared->HeapHolds() == 1u, "a texture placed by the heap counts one heap hold");
    require(heap.SlotTexture(kind, *slot) == nullptr && heap.SlotTexture(otherKind, *slot) == shared.get(),
            "the slot holds the other kind's texture");
    shared.reset();
    const auto freedBefore = heap.Statistics().slotsFreed;
    auto third = fakeTexture(0);
    require(heap.TextureSlot(kind, third, view, VK_IMAGE_LAYOUT_GENERAL).has_value(), "a texture held only by the heap is swept");
    require(heap.Statistics().slotsFreed == freedBefore + 1u, "the sweep frees the slot whose texture only the heap holds");
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
    require(!heap.ViewSlot(kind, &second, view).has_value(), "a full heap refuses another owner's view");
    const auto generation = heap.SlotGeneration(*slot);
    heap.ReleaseViewSlots(&first);
    require(heap.SlotGeneration(*slot) != generation, "a released view slot moves its generation");
    require(heap.ViewSlot(kind, &second, view) == slot, "a released view slot is reused");
    heap.ReleaseViewSlots(&second);
}

void encodedViews(const Context& context) {
    BindlessHeap heap(context);
    const auto base = heap.Statistics();
    const auto encodes = [&] { return heap.Statistics().encodes - base.encodes; };
    const auto copies = [&] { return heap.Statistics().encodedCopies - base.encodedCopies; };
    VkImageView destroyed = VK_NULL_HANDLE;
    const auto storageKey = static_cast<std::uint32_t>(VK_IMAGE_LAYOUT_GENERAL) | 0x80000000u;
    std::vector<std::byte> bytes(static_cast<std::size_t>(heap.ImageHeapStride()));
    {
        const PlaceholderImages placeholders(context);
        const auto view = placeholders.View(Abi::FirstStorageImageBinding);
        heap.AbsentSlot(Abi::FirstStorageImageBinding + 2u, view);
        require(encodes() == 1u && copies() == 0u, "a view's first heap slot encodes its descriptor");
        heap.AbsentSlot(Abi::FirstStorageImageBinding + 3u, view);
        require(encodes() == 1u && copies() == 1u, "another slot for the same view copies the encoded descriptor");
        require(CopyEncodedImageView(view, storageKey, bytes), "a live view keeps its encoded descriptor");
        destroyed = view;
    }
    require(!CopyEncodedImageView(destroyed, storageKey, bytes), "a destroyed view drops its encoded descriptor");
    const PlaceholderImages again(context);
    heap.AbsentSlot(Abi::FirstStorageImageBinding + 4u, again.View(Abi::FirstStorageImageBinding));
    require(encodes() == 2u && copies() == 1u, "a destroyed view's encoded descriptor is not copied to a new view");
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
    require(heap.StorageSlot(kind, owner(), view).has_value(), "the first storage owner takes the heap's only slot");
    require(heap.StorageSlot(kind, owner(), view).has_value() && released == 1,
            "a full heap sweeps an owner whose release calls back into the heap");
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
    constexpr std::size_t chunkWords = AgcDriver::Graphics::BindlessHeap::RingChunkBytes / sizeof(std::uint32_t);
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
    require(untilWrap() == AgcDriver::Graphics::BindlessHeap::RingChunkBytes, "the ring skips a chunk a later batch still reads kept spans from");
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

void storageComesBack(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    std::vector<std::shared_ptr<void>> owners;
    for (std::size_t index = 0; index < 7u; ++index) {
        owners.push_back(std::make_shared<int>(0));
        require(heap.StorageSlot(Abi::FirstStorageImageBinding, owners.back(), view).has_value(),
                "a storage owner takes one of the eight capped slots");
    }
    require(BindlessHeap::NearlyFull(), "storage owners that leave one free slot of eight refuse the heap");
    BindlessHeap::NoteSweepDue();
    BindlessHeap::SweepIfDue();
    require(BindlessHeap::NearlyFull(), "a sweep while every storage owner is held leaves the heap refused");
    owners.clear();
    for (std::uint32_t check = 0; check < 64u && BindlessHeap::NearlyFull(); ++check) BindlessHeap::SweepIfDue();
    require(!BindlessHeap::NearlyFull(), "a refused heap sweeps again by itself and takes the heap back once the storage owners are gone");
    BindlessHeap::Release(context.device);
}

void storageViewsComeBack(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    int released = 0;
    const auto owner = [&released] {
        return std::shared_ptr<void>(new int(0), [&released](void* pointer) {
            ++released;
            delete static_cast<int*>(pointer);
        });
    };
    std::vector<std::shared_ptr<void>> owners;
    for (std::size_t index = 0; index < 3u; ++index) {
        owners.push_back(owner());
        for (std::uint32_t kind = 0; kind < 2u; ++kind) {
            require(heap.StorageSlot(Abi::FirstStorageImageBinding + kind, owners.back(), view).has_value(),
                    "a storage owner takes one of the eight capped slots for each of its two views");
        }
    }
    owners.push_back(owner());
    require(heap.StorageSlot(Abi::FirstStorageImageBinding, owners.back(), view).has_value(), "a single-view storage owner takes the seventh slot");
    require(BindlessHeap::NearlyFull(), "storage owners that leave one free slot of eight refuse the heap");
    owners.clear();
    BindlessHeap::NoteSweepDue();
    BindlessHeap::SweepIfDue();
    require(released == 4, "a sweep released " + std::to_string(released) + " of four storage owners the heap alone held, three of them in two slots each");
    require(!BindlessHeap::NearlyFull(), "a sweep after every storage owner is gone takes the heap again");

    const auto image = [&released](std::size_t index, bool cached) {
        auto* pointer = reinterpret_cast<StorageTexture*>(fakeImages[index].data());
        pointer->SetCached(true);
        pointer->SetCached(cached);
        return std::shared_ptr<StorageTexture>(pointer, [&released](StorageTexture*) { ++released; });
    };
    auto evicted = image(0, true);
    auto cached = image(1, true);
    for (std::uint32_t kind = 0; kind < 2u; ++kind) require(heap.StorageSlot(Abi::FirstStorageImageBinding + kind, evicted, view).has_value(), "an evicted image's view takes a slot");
    require(heap.StorageSlot(Abi::FirstStorageImageBinding, cached, view).has_value(), "a cached image takes a slot");
    evicted->SetCached(false);
    BindlessHeap::NoteStorageEvicted();
    BindlessHeap::SweepIfDue();
    require(released == 4, "an evicted storage image still held outside the heap lost its slots");
    evicted.reset();
    cached.reset();
    BindlessHeap::SweepIfDue();
    require(released == 5, "the present after an evicted storage image's last outside holder went did not release its two slots, or released the cached image the heap alone holds");

    auto late = image(0, false);
    require(heap.StorageSlot(Abi::FirstStorageImageBinding, late, view).has_value(), "an image that left the storage cache before its first slot takes one");
    BindlessHeap::SweepIfDue();
    require(released == 5, "an image that left the cache lost its slot while still held outside the heap");
    late.reset();
    BindlessHeap::SweepIfDue();
    require(released == 6, "an image that took its slot after leaving the storage cache kept it after its last outside holder went");

    auto outside = std::shared_ptr<StorageTexture>(reinterpret_cast<StorageTexture*>(fakeImages[2].data()), [&released](StorageTexture*) { ++released; });
    require(heap.StorageSlot(Abi::FirstStorageImageBinding, outside, view).has_value(), "an image made outside the storage cache takes a slot");
    outside.reset();
    BindlessHeap::NoteStorageEvicted();
    BindlessHeap::SweepIfDue();
    require(released == 6, "an image the storage cache never held lost its slot without a sweep");
    BindlessHeap::Release(context.device);
}

void mixedComesBack(const Context& context, VkImageView sampledView, VkImageView storageView) {
    auto& heap = BindlessHeap::For(context);
    std::vector<std::shared_ptr<Texture>> textures;
    for (std::size_t index = 0; index < 4u; ++index) {
        textures.push_back(fakeTexture(index));
        require(heap.TextureSlot(Abi::FirstImageBinding, textures.back(), sampledView, VK_IMAGE_LAYOUT_GENERAL).has_value(),
                "a texture takes one of the eight capped slots");
    }
    require(!BindlessHeap::NearlyFull(), "four textures in eight capped slots leave the heap taken");
    std::vector<std::shared_ptr<void>> owners;
    for (std::size_t index = 0; index < 3u; ++index) {
        owners.push_back(std::make_shared<int>(0));
        require(heap.StorageSlot(Abi::FirstStorageImageBinding, owners.back(), storageView).has_value(),
                "a storage owner takes one of the slots the textures left");
    }
    require(BindlessHeap::NearlyFull(), "storage owners that take the slots the textures left refuse the heap");
    owners.clear();
    BindlessHeap::NoteSweepDue();
    BindlessHeap::SweepIfDue();
    require(!BindlessHeap::NearlyFull(), "a sweep after the storage owners are gone takes the heap again while the textures stay");
    BindlessHeap::Release(context.device);
}

void viewsComeBack(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    const auto kind = Abi::FirstStorageImageBinding + 27u;
    std::array<int, 7> owners{};
    for (auto& owner : owners) require(heap.ViewSlot(kind, &owner, view).has_value(), "a view takes one of the eight capped slots");
    require(BindlessHeap::NearlyFull(), "views that leave one free slot of eight refuse the heap");
    for (auto& owner : owners) heap.ReleaseViewSlots(&owner);
    BindlessHeap::SweepIfDue();
    require(!BindlessHeap::NearlyFull(), "releasing the views schedules the sweep that takes the heap again");
    BindlessHeap::Release(context.device);
}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    const bool comesBack = !mode.empty();
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
                const auto sampled = placeholders.View(Abi::FirstImageBinding);
                const auto storage = placeholders.View(Abi::FirstStorageImageBinding);
                if (mode == "comes-back") heapComesBack(context, sampled);
                else if (mode == "storage-comes-back") storageComesBack(context, storage);
                else if (mode == "storage-views-come-back") storageViewsComeBack(context, storage);
                else if (mode == "mixed-comes-back") mixedComesBack(context, sampled, storage);
                else if (mode == "views-come-back") viewsComeBack(context, storage);
                else throw std::runtime_error("unknown mode " + mode);
                device->WaitIdle();
            }
            device.reset();
            std::cout << "the refused heap comes back after a sweep (" << mode << ")\n";
            return 0;
        }
        {
            const auto context = device->GraphicsContext();
            const PlaceholderImages placeholders(context);
            textureSlots(context, placeholders.View(Abi::FirstImageBinding));
            samplerSlots(context);
            ringSpans(context);
            viewSlots(context, placeholders.View(Abi::FirstStorageImageBinding));
            encodedViews(context);
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
