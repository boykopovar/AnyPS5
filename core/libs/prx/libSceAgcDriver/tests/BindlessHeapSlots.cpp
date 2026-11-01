#include "execution/VulkanTestDevice.hpp"
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

void require(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error(what);
}

struct FakeOwner : HeapSlotOwner {
    FakeOwner() = default;
};

alignas(Texture) std::array<std::array<std::byte, sizeof(Texture)>, 8> fakeTextures{};

std::shared_ptr<Texture> fakeTexture(std::size_t index) {
    return std::shared_ptr<Texture>(reinterpret_cast<Texture*>(fakeTextures[index].data()), [](Texture*) {});
}

void textureSlots(const Context& context, VkImageView view) {
    BindlessHeap heap(context);
    const auto kind = Abi::FirstImageBinding;
    auto first = fakeTexture(0);
    const auto slot = heap.TextureSlot(kind, first.get(), view, VK_IMAGE_LAYOUT_GENERAL);
    require(slot.has_value(), "the first texture takes a slot");
    require(heap.SlotTexture(kind, *slot) == first.get(), "the slot names the first texture");
    require(first.use_count() == 1, "a heap slot holds a reference to its texture");
    const auto generation = heap.SlotGeneration(*slot);

    auto second = fakeTexture(1);
    require(!heap.TextureSlot(kind, second.get(), view, VK_IMAGE_LAYOUT_GENERAL).has_value(), "the slot of a live texture is not reused");
    heap.ReleaseSlots(first.get());
    const auto reused = heap.TextureSlot(kind, second.get(), view, VK_IMAGE_LAYOUT_GENERAL);
    require(reused.has_value() && *reused == *slot, "the released texture's slot is reused");
    require(heap.SlotGeneration(*slot) != generation, "a freed slot moves its generation");
    require(heap.SlotTexture(kind, *slot) == second.get(), "the reused slot names the second texture");
    const auto counts = heap.Statistics();
    require(counts.slotsFreed == 1u && counts.slotsReused == 1u, "one slot freed and reused");

    auto shared = fakeTexture(2);
    const auto otherKind = kind + 1u;
    require(!heap.TextureSlot(otherKind, shared.get(), view, VK_IMAGE_LAYOUT_GENERAL).has_value(),
            "another kind finds the capped heap's only slot taken");
    heap.ReleaseSlots(second.get());
    const auto sharedSlot = heap.TextureSlot(otherKind, shared.get(), view, VK_IMAGE_LAYOUT_GENERAL);
    require(sharedSlot.has_value() && *sharedSlot == *slot, "another kind takes the slot the first kind's texture left");
    require(heap.SlotTexture(kind, *slot) == nullptr && heap.SlotTexture(otherKind, *slot) == shared.get(),
            "the slot names the other kind's texture");
    const auto freedBefore = heap.Statistics().slotsFreed;
    heap.ReleaseSlots(shared.get());
    heap.ReleaseSlots(shared.get());
    require(heap.Statistics().slotsFreed == freedBefore + 1u, "a texture's release frees its slot once");
}

void samplerSlots(const Context& context) {
    auto& heap = BindlessHeap::For(context);
    auto cache = std::make_unique<SamplerCache>();
    const std::array<std::uint32_t, 4> firstWords{0u, 0u, 0u, 0u};
    const std::array<std::uint32_t, 4> secondWords{0u, 0u, 0x00100000u, 0u};
    auto first = cache->Get(context, firstWords, false);
    const auto uses = first.use_count();
    const auto slot = heap.SamplerSlot(first);
    require(slot.has_value(), "the first sampler takes a slot");
    require(first.use_count() == uses, "a heap slot holds a reference to its sampler");
    const auto generation = heap.SamplerGeneration(*slot);
    require(first->CacheKey() != nullptr, "a cached sampler carries its key");
    first.reset();
    cache.reset();
    require(heap.SamplerGeneration(*slot) != generation, "a destroyed sampler did not release its slot");
    cache = std::make_unique<SamplerCache>();
    auto second = cache->Get(context, secondWords, false);
    const auto reused = heap.SamplerSlot(second);
    require(reused.has_value() && *reused == *slot, "the destroyed sampler's slot is reused");
}

void viewSlots(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    const auto kind = Abi::FirstStorageImageBinding + 27u;
    FakeOwner first, second;
    const auto slot = heap.ViewSlot(kind, &first, view);
    require(slot.has_value() && heap.ViewSlot(kind, &first, view) == slot, "a view keeps its slot");
    require(!heap.ViewSlot(kind, &second, view).has_value(), "a full heap refuses another owner's view");
    const auto generation = heap.SlotGeneration(*slot);
    heap.ReleaseSlots(&first);
    require(heap.SlotGeneration(*slot) != generation, "a released view slot moves its generation");
    require(heap.ViewSlot(kind, &second, view) == slot, "a released view slot is reused");
    heap.ReleaseSlots(&second);
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

void ownerSlots(const Context& context, VkImageView view) {
    BindlessHeap heap(context);
    FakeOwner image, other;
    const auto slot = heap.StorageSlot(Abi::FirstStorageImageBinding, &image, view);
    require(slot.has_value(), "a storage owner takes the capped heap's only slot");
    require(!heap.StorageSlot(Abi::FirstStorageImageBinding + 1u, &image, view).has_value(), "the owner's second view finds the heap full");
    heap.ReleaseSlots(&other);
    require(!heap.StorageSlot(Abi::FirstStorageImageBinding, &other, view).has_value(), "another owner's release freed this owner's slot");
    heap.ReleaseSlots(&image);
    require(heap.StorageSlot(Abi::FirstStorageImageBinding, &other, view) == slot, "a storage owner's release frees its slot");
    heap.ReleaseSlots(&other);
}

void ownerViewsRelease(const Context& context, VkImageView view) {
    BindlessHeap heap(context);
    FakeOwner owner;
    const auto firstKind = Abi::FirstStorageImageBinding, secondKind = Abi::FirstStorageImageBinding + 1u;
    const auto first = heap.StorageSlot(firstKind, &owner, view);
    const auto second = heap.ViewSlot(secondKind, &owner, view);
    require(first.has_value() && second.has_value() && *first != *second, "one owner takes a slot for each of its two views");
    const auto before = heap.Statistics();
    const auto serial = heap.SlotSerial();
    require(heap.StorageSlot(firstKind, &owner, view) == first && heap.ViewSlot(secondKind, &owner, view) == second, "a lookup hit returns the owner's existing slots");
    const auto hit = heap.Statistics();
    require(hit.slotsFreed == before.slotsFreed && hit.slotsReused == before.slotsReused && hit.encodes == before.encodes && hit.encodedCopies == before.encodedCopies,
            "a lookup hit changed the counts");
    require(heap.SlotSerial() == serial, "a lookup hit moved the slot serial");
    const auto firstGeneration = heap.SlotGeneration(*first), secondGeneration = heap.SlotGeneration(*second);
    heap.ReleaseSlots(&owner);
    const auto released = heap.Statistics();
    require(released.slotsFreed == before.slotsFreed + 2u, "one release freed " + std::to_string(released.slotsFreed - before.slotsFreed) + " of the owner's two slots");
    require(heap.SlotGeneration(*first) != firstGeneration && heap.SlotGeneration(*second) != secondGeneration, "the release moved both slots' generations");
    require(heap.SlotSerial() != serial, "a release moved the slot serial");
    const auto releasedSerial = heap.SlotSerial();
    const auto releasedGenerations = std::array{heap.SlotGeneration(*first), heap.SlotGeneration(*second)};
    heap.ReleaseSlots(&owner);
    require(heap.Statistics().slotsFreed == released.slotsFreed, "a second release of the same owner freed slots");
    require(heap.SlotSerial() == releasedSerial, "a second release moved the slot serial");
    require(heap.SlotGeneration(*first) == releasedGenerations[0] && heap.SlotGeneration(*second) == releasedGenerations[1], "a second release moved a generation");
}

struct SharedOwner : std::enable_shared_from_this<SharedOwner>, HeapSlotOwner {
    explicit SharedOwner(const Context& context) : context(context) {}
    ~SharedOwner() { ReleaseHeapSlots(context); }
    Context context;
};

void sharedOwnerRelease(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    auto owner = std::make_shared<SharedOwner>(context);
    const HeapSlotOwner* base = owner.get();
    require(static_cast<const void*>(base) != static_cast<const void*>(owner.get()), "the slot owner base does not sit behind another base");
    const auto slot = heap.StorageSlot(Abi::FirstStorageImageBinding, owner.get(), view);
    require(slot.has_value(), "an owner behind another base takes a slot");
    const auto generation = heap.SlotGeneration(*slot);
    const auto freed = heap.Statistics().slotsFreed;
    owner.reset();
    require(heap.SlotGeneration(*slot) != generation && heap.Statistics().slotsFreed == freed + 1u,
            "destroying an owner behind another base did not free its slot");
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
    require(heap.Held(crossing), "a span over two chunks is kept");
    require(heap.Held(first), "a span stays held while its batch keeps the chunk open");
    const auto counts = heap.Statistics();
    require(counts.spansWritten == 3u && counts.spansKept == 3u, "three spans written and kept");
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

void ringLargeSpans(const Context& context) {
    BindlessHeap heap(context);
    require(heap.RingChunkCount() >= 5u, "the ring has fewer than five chunks, so a span over three chunks cannot be tested");
    constexpr std::size_t chunkWords = AgcDriver::Graphics::BindlessHeap::RingChunkBytes / sizeof(std::uint32_t);
    constexpr std::size_t largeWords = chunkWords * 2u + 1024u;
    BindlessHeap::SpanMark pin;
    static_cast<void>(heap.Allocate(16u, pin));
    BindlessHeap::SpanMark large;
    const auto span = heap.Allocate(largeWords, large);
    require(span.words.size() == largeWords && heap.Held(large), "a span over three chunks is written and held");
    Recorder recorder(context);
    BindlessHeap::SpanMark fill;
    const auto untilWrap = [&](const BindlessHeap::SpanMark& kept) {
        const auto wraps = heap.Statistics().ringWraps;
        BindlessHeap::DataSpan last{};
        while (heap.Statistics().ringWraps == wraps) {
            heap.Submitted(recorder, 0u);
            require(heap.Held(kept), "a kept span stays held while later batches fill the ring");
            last = heap.Allocate(chunkWords, fill);
        }
        return last.offset;
    };
    require(untilWrap(large) == 3u * AgcDriver::Graphics::BindlessHeap::RingChunkBytes, "the ring skips every chunk a kept large span covers");
    require(untilWrap(pin) == AgcDriver::Graphics::BindlessHeap::RingChunkBytes, "the ring skips only the pinned first chunk");
    require(!heap.Held(large), "a large span expires once the ring writes over one of its later chunks");
    require(heap.Held(pin), "the pinned span outlives the large span");
    bool refused = false;
    try {
        static_cast<void>(heap.Allocate(AgcDriver::Graphics::BindlessHeap::RingBytes / sizeof(std::uint32_t) / 2u + 1u));
    } catch (const std::runtime_error&) {
        refused = true;
    }
    require(refused, "a span over half the ring is refused");
}

void ringCurrentChunk(const Context& context) {
    BindlessHeap heap(context);
    constexpr std::size_t chunkWords = AgcDriver::Graphics::BindlessHeap::RingChunkBytes / sizeof(std::uint32_t);
    Recorder recorder(context);
    std::vector<BindlessHeap::SpanMark> pins(heap.RingChunkCount() - 1u);
    for (auto& pin : pins) {
        static_cast<void>(heap.Allocate(chunkWords, pin));
        heap.Submitted(recorder, 0u);
        for (const auto& kept : pins) {
            if (kept.entry != 0u) require(heap.Held(kept), "every pinned chunk stays held");
        }
    }
    BindlessHeap::SpanMark live;
    static_cast<void>(heap.Allocate(16u, live));
    bool refused = false;
    try {
        static_cast<void>(heap.Allocate(chunkWords));
    } catch (const std::runtime_error&) {
        refused = true;
    }
    require(refused, "a span never wraps onto the current chunk behind data this batch already wrote");
    require(heap.Held(live), "the current chunk's span is still held");
}

void heapComesBack(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    const auto kind = Abi::FirstImageBinding;
    std::vector<std::shared_ptr<Texture>> held;
    for (std::size_t index = 0; index < 7u; ++index) {
        held.push_back(fakeTexture(index));
        require(heap.TextureSlot(kind, held.back().get(), view, VK_IMAGE_LAYOUT_GENERAL).has_value(),
                "a texture takes one of the eight capped slots");
    }
    require(BindlessHeap::NearlyFull(), "one free slot of eight refuses the heap");
    for (std::size_t index = 0; index < 2u; ++index) heap.ReleaseSlots(held[index].get());
    require(BindlessHeap::NearlyFull(), "three free slots of eight leave the heap refused");
    heap.ReleaseSlots(held[2].get());
    require(!BindlessHeap::NearlyFull(), "a release that leaves twice the margin free takes the heap again");
    for (std::size_t index = 3; index < held.size(); ++index) heap.ReleaseSlots(held[index].get());
    BindlessHeap::Release(context.device);
}

void storageComesBack(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    std::array<FakeOwner, 7> owners{};
    for (auto& owner : owners) {
        require(heap.StorageSlot(Abi::FirstStorageImageBinding, &owner, view).has_value(),
                "a storage owner takes one of the eight capped slots");
    }
    require(BindlessHeap::NearlyFull(), "storage owners that leave one free slot of eight refuse the heap");
    for (auto& owner : owners) heap.ReleaseSlots(&owner);
    require(!BindlessHeap::NearlyFull(), "releasing the storage owners takes the heap again");
    BindlessHeap::Release(context.device);
}

void storageViewsComeBack(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    std::array<FakeOwner, 4> owners{};
    for (std::size_t index = 0; index < 3u; ++index) {
        for (std::uint32_t kind = 0; kind < 2u; ++kind) {
            require(heap.StorageSlot(Abi::FirstStorageImageBinding + kind, &owners[index], view).has_value(),
                    "a storage owner takes one of the eight capped slots for each of its two views");
        }
    }
    require(heap.StorageSlot(Abi::FirstStorageImageBinding, &owners[3], view).has_value(), "a single-view storage owner takes the seventh slot");
    require(BindlessHeap::NearlyFull(), "storage owners that leave one free slot of eight refuse the heap");
    const auto freed = heap.Statistics().slotsFreed;
    heap.ReleaseSlots(&owners[0]);
    require(heap.Statistics().slotsFreed == freed + 2u, "a storage owner's release freed " + std::to_string(heap.Statistics().slotsFreed - freed) + " of its two slots");
    require(BindlessHeap::NearlyFull(), "three free slots of eight leave the heap refused");
    heap.ReleaseSlots(&owners[1]);
    require(!BindlessHeap::NearlyFull(), "releasing a second two-view owner takes the heap again");
    heap.ReleaseSlots(&owners[2]);
    heap.ReleaseSlots(&owners[3]);
    BindlessHeap::Release(context.device);
}

void mixedComesBack(const Context& context, VkImageView sampledView, VkImageView storageView) {
    auto& heap = BindlessHeap::For(context);
    std::vector<std::shared_ptr<Texture>> textures;
    for (std::size_t index = 0; index < 4u; ++index) {
        textures.push_back(fakeTexture(index));
        require(heap.TextureSlot(Abi::FirstImageBinding, textures.back().get(), sampledView, VK_IMAGE_LAYOUT_GENERAL).has_value(),
                "a texture takes one of the eight capped slots");
    }
    require(!BindlessHeap::NearlyFull(), "four textures in eight capped slots leave the heap taken");
    std::array<FakeOwner, 3> owners{};
    for (auto& owner : owners) {
        require(heap.StorageSlot(Abi::FirstStorageImageBinding, &owner, storageView).has_value(),
                "a storage owner takes one of the slots the textures left");
    }
    require(BindlessHeap::NearlyFull(), "storage owners that take the slots the textures left refuse the heap");
    for (auto& owner : owners) heap.ReleaseSlots(&owner);
    require(!BindlessHeap::NearlyFull(), "releasing the storage owners takes the heap again while the textures stay");
    for (const auto& texture : textures) heap.ReleaseSlots(texture.get());
    BindlessHeap::Release(context.device);
}

void viewsComeBack(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    const auto kind = Abi::FirstStorageImageBinding + 27u;
    std::array<FakeOwner, 7> owners{};
    for (auto& owner : owners) require(heap.ViewSlot(kind, &owner, view).has_value(), "a view takes one of the eight capped slots");
    require(BindlessHeap::NearlyFull(), "views that leave one free slot of eight refuse the heap");
    for (auto& owner : owners) heap.ReleaseSlots(&owner);
    require(!BindlessHeap::NearlyFull(), "releasing the views takes the heap again");
    BindlessHeap::Release(context.device);
}

void twoHeapsComeBack(const Context& context, VkImageView view) {
    auto first = std::make_unique<BindlessHeap>(context);
    auto second = std::make_unique<BindlessHeap>(context);
    std::array<FakeOwner, 7> firstOwners{}, secondOwners{};
    for (auto& owner : firstOwners) require(first->ViewSlot(Abi::FirstStorageImageBinding, &owner, view).has_value(), "a view takes one of the first heap's eight capped slots");
    for (auto& owner : secondOwners) require(second->ViewSlot(Abi::FirstStorageImageBinding, &owner, view).has_value(), "a view takes one of the second heap's eight capped slots");
    require(BindlessHeap::NearlyFull(), "two nearly full heaps refuse the heap");
    for (auto& owner : secondOwners) second->ReleaseSlots(&owner);
    require(BindlessHeap::NearlyFull(), "room in the second heap took the heap back while the first stays nearly full");
    second.reset();
    require(BindlessHeap::NearlyFull(), "destroying the second heap took the heap back while the first stays nearly full");
    for (auto& owner : firstOwners) first->ReleaseSlots(&owner);
    require(!BindlessHeap::NearlyFull(), "room in the last refused heap did not take the heap back");
    first.reset();
}

}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    const bool comesBack = !mode.empty();
    try {
        SetTestEnvironment("APS5_GPU_HEAP", "1");
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->graphicsContext().descriptorHeap) {
            std::printf("skipped, the device has no VK_EXT_descriptor_heap\n");
            return VulkanTestSkipped;
        }
        SetTestEnvironment("APS5_HEAP_SLOT_CAP", comesBack ? "8" : "1");
        if (comesBack) {
            {
                const auto context = device->graphicsContext();
                const PlaceholderImages placeholders(context);
                const auto sampled = placeholders.View(Abi::FirstImageBinding);
                const auto storage = placeholders.View(Abi::FirstStorageImageBinding);
                if (mode == "comes-back") heapComesBack(context, sampled);
                else if (mode == "storage-comes-back") storageComesBack(context, storage);
                else if (mode == "storage-views-come-back") storageViewsComeBack(context, storage);
                else if (mode == "mixed-comes-back") mixedComesBack(context, sampled, storage);
                else if (mode == "views-come-back") viewsComeBack(context, storage);
                else if (mode == "two-heaps-come-back") twoHeapsComeBack(context, storage);
                else if (mode == "owner-views-release") ownerViewsRelease(context, storage);
                else if (mode == "shared-owner-release") sharedOwnerRelease(context, storage);
                else throw std::runtime_error("unknown mode " + mode);
                device->WaitIdle();
            }
            device.reset();
            std::cout << "the refused heap comes back once released slots leave room (" << mode << ")\n";
            return 0;
        }
        {
            const auto context = device->graphicsContext();
            const PlaceholderImages placeholders(context);
            textureSlots(context, placeholders.View(Abi::FirstImageBinding));
            samplerSlots(context);
            ringSpans(context);
            ringLargeSpans(context);
            ringCurrentChunk(context);
            viewSlots(context, placeholders.View(Abi::FirstStorageImageBinding));
            encodedViews(context);
            ownerSlots(context, placeholders.View(Abi::FirstStorageImageBinding));
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
