#include "execution/VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DescriptorHeapObjects.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "RuntimeAbi.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
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

alignas(Texture) std::array<std::array<std::byte, sizeof(Texture)>, 12> fakeTextures{};

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

void writeTails(const Context& context) {
    BindlessHeap heap(context);
    for (const std::size_t count : {0u, 1u, 3u, 4u, 5u, 7u, 9u}) {
        std::vector<std::uint32_t> words(count);
        for (std::size_t index = 0; index < count; ++index) words[index] = 0x1000u * static_cast<std::uint32_t>(count) + static_cast<std::uint32_t>(index) + 1u;
        BindlessHeap::SpanMark mark;
        const auto span = heap.Allocate(count + 1u, mark);
        span.words[count] = 0xdeadbeefu;
        BindlessHeap::Write(span, words);
        require(std::equal(words.begin(), words.end(), span.words.begin()), "a GPU heap span holds other words than were written");
        require(span.words[count] == 0xdeadbeefu, "writing a GPU heap span wrote past its words");
    }
}

void spanScope(const Context& context) {
    BindlessHeap heap(context);
    BindlessHeap::SpanMark first;
    std::atomic<bool> allocated{false};
    std::thread other;
    bool waited = false;
    {
        const BindlessHeap::SpanScope outer(heap);
        const BindlessHeap::SpanScope inner(heap);
        static_cast<void>(heap.Allocate(16u, first));
        require(heap.Held(first) && !heap.KeepOrAllocate(inner, 16u, first, true), "spans are allocated and kept inside nested scopes");
        other = std::thread([&] {
            BindlessHeap::SpanMark mark;
            static_cast<void>(heap.Allocate(16u, mark));
            allocated.store(true, std::memory_order_release);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        waited = !allocated.load(std::memory_order_acquire);
    }
    other.join();
    require(waited, "another thread's span waits while a scope holds the allocator");
    require(allocated.load(std::memory_order_acquire) && heap.Held(first), "the other thread allocates once the scope ends");
    require(heap.Statistics().spansWritten == 2u, "both spans are counted");
}

bool dataPool(const Context& context) {
    Recorder recorder(context, true);
    if (!recorder.HasTimeline()) return false;
    constexpr std::size_t blockWords = AgcDriver::Graphics::BindlessHeap::PoolBlockBytes / sizeof(std::uint32_t);
    {
        BindlessHeap heap(context);
        std::vector<BindlessHeap::SpanMark> marks(5);
        for (auto& mark : marks) static_cast<void>(heap.Allocate(blockWords / 2u, mark));
        require(heap.Statistics().poolBlocksMade == 3u, "one batch's shader data grows into new blocks instead of waiting");
        for (const auto& mark : marks) require(heap.Held(mark), "every span of the batch is held");
        BindlessHeap::SpanMark big;
        const auto span = heap.Allocate(blockWords * 3u / 2u, big);
        require(span.words.size() == blockWords * 3u / 2u && heap.Statistics().poolBlocksMade == 4u, "a span larger than a block gets a block of its own");
        const auto [buffer, offset] = heap.DataLocation(span.address + 64u);
        require(buffer != VK_NULL_HANDLE && offset == span.offset + 64u, "a pooled address maps back to its buffer and offset");
        heap.Submitted(recorder, recorder.Submissions() + (1ull << 40u));
        const auto made = heap.Statistics().poolBlocksMade;
        BindlessHeap::SpanMark fill;
        for (int index = 0; index < 4; ++index) static_cast<void>(heap.Allocate(blockWords / 2u + 1u, fill));
        const auto busy = heap.Statistics();
        require(busy.poolBlocksRecycled == 0u && busy.poolBlocksMade > made, "blocks a batch in flight still reads are not reused");
    }
    {
        BindlessHeap heap(context);
        BindlessHeap::SpanMark kept;
        BindlessHeap::SpanMark first;
        static_cast<void>(heap.Allocate(blockWords - 64u, kept));
        static_cast<void>(heap.Allocate(blockWords - 64u, first));
        static_cast<void>(heap.Allocate(blockWords - 64u));
        heap.Submitted(recorder, 0u);
        require(heap.Held(kept), "a span a later draw reuses stays held");
        const auto made = heap.Statistics().poolBlocksMade;
        BindlessHeap::SpanMark fill;
        static_cast<void>(heap.Allocate(blockWords - 64u, fill));
        const auto counts = heap.Statistics();
        require(counts.poolBlocksRecycled == 1u && counts.poolBlocksMade == made, "a completed block is reused instead of making a new one");
        require(heap.Held(kept), "the block a kept span reopened is not reused");
        require(!heap.Held(first), "a span in a reused block has expired");
    }
    {
        BindlessHeap heap(context);
        constexpr std::size_t blocks = 2u * AgcDriver::Graphics::BindlessHeap::PoolIdleBytes / AgcDriver::Graphics::BindlessHeap::PoolBlockBytes;
        for (std::size_t index = 0; index < blocks; ++index) static_cast<void>(heap.Allocate(blockWords - 64u));
        heap.Submitted(recorder, 0u);
        const auto counts = heap.Statistics();
        require(counts.poolBlocksFreed != 0u && counts.poolBlockBytes <= AgcDriver::Graphics::BindlessHeap::PoolIdleBytes + AgcDriver::Graphics::BindlessHeap::PoolBlockBytes,
                "idle blocks beyond the pool's budget are freed");
    }
    {
        BindlessHeap heap(context);
        const BindlessHeap::SpanScope spans(heap);
        BindlessHeap::TableMark table;
        const std::vector<std::uint32_t> words(40u, 0x5au);
        const auto first = heap.AllocateTable(spans, words.size(), table);
        require(first.has_value() && table.address == first->address && first->words.size() == words.size(), "a stage table is allocated");
        BindlessHeap::Write(*first, words);
        const auto firstAddress = table.address;
        heap.RetireTable(spans, table);
        require(table.address == 0u, "a retired table leaves its owner");
        BindlessHeap::TableMark unsubmitted;
        const auto second = heap.AllocateTable(spans, words.size(), unsubmitted);
        require(second.has_value() && second->address != firstAddress, "a table retired in the open batch is not reused before the batch is submitted");
        heap.Submitted(recorder, recorder.Submissions() + (1ull << 40u));
        BindlessHeap::TableMark busy;
        const auto third = heap.AllocateTable(spans, words.size(), busy);
        require(third.has_value() && third->address != firstAddress, "a table a batch in flight still reads is not reused");
        heap.RetireTable(spans, unsubmitted);
        heap.Submitted(recorder, 0u);
        BindlessHeap::TableMark reused;
        const auto fourth = heap.AllocateTable(spans, words.size(), reused);
        require(fourth.has_value() && fourth->address != firstAddress, "a table waits for the batch that last read it, not a later one");
        BindlessHeap::TableMark large;
        const auto big = heap.AllocateTable(spans, AgcDriver::Graphics::BindlessHeap::TableBlockBytes, large);
        require(big.has_value() && big->words.size() == AgcDriver::Graphics::BindlessHeap::TableBlockBytes && heap.DataLocation(large.address).first != VK_NULL_HANDLE, "a table larger than a shared block gets a block of its own");
    }
    {
        BindlessHeap heap(context);
        const BindlessHeap::SpanScope spans(heap);
        BindlessHeap::TableMark table;
        static_cast<void>(heap.AllocateTable(spans, 40u, table));
        const auto address = table.address;
        heap.RetireTable(spans, table);
        heap.Submitted(recorder, 0u);
        BindlessHeap::TableMark next;
        require(heap.AllocateTable(spans, 40u, next).has_value() && next.address == address, "a table whose batch completed is reused");
    }
    return true;
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
    require(BindlessHeap::NearlyFull(), "one free slot of eight marks the heap nearly full");
    for (std::size_t index = 0; index < 2u; ++index) heap.ReleaseSlots(held[index].get());
    require(BindlessHeap::NearlyFull(), "three free slots of eight leave the heap nearly full");
    heap.ReleaseSlots(held[2].get());
    require(!BindlessHeap::NearlyFull(), "a release that leaves twice the margin free clears the nearly full state");
    for (std::size_t index = 3; index < held.size(); ++index) heap.ReleaseSlots(held[index].get());
    BindlessHeap::Release(context.device);
}

bool idleSlotsEvicted(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    const auto kind = Abi::FirstImageBinding;
    Recorder recorder(context, true);
    if (!recorder.HasTimeline()) return false;
    recorder.Activate();
    const auto submit = [&] {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        static_cast<void>(recorder.Commands());
        recorder.Submit();
    };
    std::vector<std::shared_ptr<Texture>> held;
    std::vector<std::uint32_t> slots;
    const auto take = [&](std::size_t index) {
        if (held.size() <= index) held.resize(index + 1u);
        if (held[index] == nullptr) held[index] = fakeTexture(index);
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        return heap.TextureSlot(kind, held[index].get(), view, VK_IMAGE_LAYOUT_GENERAL);
    };
    for (std::size_t index = 0; index < 6u; ++index) {
        const auto slot = take(index);
        require(slot.has_value(), "a texture takes one of the eight capped slots");
        slots.push_back(*slot);
    }
    const auto idleGeneration = heap.SlotGeneration(slots[0]);
    submit();
    submit();
    require(take(5) == slots[5], "a texture a draw uses again keeps its slot");
    require(take(6).has_value(), "the seventh texture takes a slot");
    require(heap.Statistics().slotsEvicted == 5u, "the five slots idle for two submissions are evicted");
    require(heap.SlotTexture(kind, slots[0]) == nullptr && heap.SlotGeneration(slots[0]) != idleGeneration,
            "an evicted slot names no texture and moves its generation");
    require(heap.SlotTexture(kind, slots[5]) == held[5].get(), "the slot used again stays with its texture");
    require(BindlessHeap::NearlyFull(), "evicted slots no submission covers yet do not count as free");
    require(take(7).has_value(), "the eighth texture takes the last never-used slot");
    require(!take(8).has_value(), "an evicted slot is not reused before a submission covers the draws that used it");
    submit();
    require(!BindlessHeap::NearlyFull(), "evicted slots a submission covers clear the nearly full state");
    require(recorder.WaitSerialFor(recorder.Submissions(), ~std::uint64_t{0}), "the submission completes");
    const auto reused = take(8);
    require(reused.has_value() && std::find(slots.begin(), slots.begin() + 5, *reused) != slots.begin() + 5,
            "an evicted slot is reused once the submission after its eviction completes");
    const auto back = take(0);
    require(back.has_value() && heap.SlotTexture(kind, *back) == held[0].get(), "an evicted texture takes a slot again when a draw uses it");
    for (const auto& texture : held) heap.ReleaseSlots(texture.get());
    BindlessHeap::Release(context.device);
    return true;
}

void storageComesBack(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    std::array<FakeOwner, 7> owners{};
    for (auto& owner : owners) {
        require(heap.StorageSlot(Abi::FirstStorageImageBinding, &owner, view).has_value(),
                "a storage owner takes one of the eight capped slots");
    }
    require(BindlessHeap::NearlyFull(), "storage owners that leave one free slot of eight mark the heap nearly full");
    for (auto& owner : owners) heap.ReleaseSlots(&owner);
    require(!BindlessHeap::NearlyFull(), "releasing the storage owners clears the nearly full state");
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
    require(BindlessHeap::NearlyFull(), "storage owners that leave one free slot of eight mark the heap nearly full");
    const auto freed = heap.Statistics().slotsFreed;
    heap.ReleaseSlots(&owners[0]);
    require(heap.Statistics().slotsFreed == freed + 2u, "a storage owner's release freed " + std::to_string(heap.Statistics().slotsFreed - freed) + " of its two slots");
    require(BindlessHeap::NearlyFull(), "three free slots of eight leave the heap nearly full");
    heap.ReleaseSlots(&owners[1]);
    require(!BindlessHeap::NearlyFull(), "releasing a second two-view owner clears the nearly full state");
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
    require(!BindlessHeap::NearlyFull(), "four textures in eight capped slots do not mark the heap nearly full");
    std::array<FakeOwner, 3> owners{};
    for (auto& owner : owners) {
        require(heap.StorageSlot(Abi::FirstStorageImageBinding, &owner, storageView).has_value(),
                "a storage owner takes one of the slots the textures left");
    }
    require(BindlessHeap::NearlyFull(), "storage owners that take the slots the textures left mark the heap nearly full");
    for (auto& owner : owners) heap.ReleaseSlots(&owner);
    require(!BindlessHeap::NearlyFull(), "releasing the storage owners clears the nearly full state while the textures stay");
    for (const auto& texture : textures) heap.ReleaseSlots(texture.get());
    BindlessHeap::Release(context.device);
}

void viewsComeBack(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    const auto kind = Abi::FirstStorageImageBinding + 27u;
    std::array<FakeOwner, 7> owners{};
    for (auto& owner : owners) require(heap.ViewSlot(kind, &owner, view).has_value(), "a view takes one of the eight capped slots");
    require(BindlessHeap::NearlyFull(), "views that leave one free slot of eight mark the heap nearly full");
    for (auto& owner : owners) heap.ReleaseSlots(&owner);
    require(!BindlessHeap::NearlyFull(), "releasing the views clears the nearly full state");
    BindlessHeap::Release(context.device);
}

void twoHeapsComeBack(const Context& context, VkImageView view) {
    auto first = std::make_unique<BindlessHeap>(context);
    auto second = std::make_unique<BindlessHeap>(context);
    std::array<FakeOwner, 7> firstOwners{}, secondOwners{};
    for (auto& owner : firstOwners) require(first->ViewSlot(Abi::FirstStorageImageBinding, &owner, view).has_value(), "a view takes one of the first heap's eight capped slots");
    for (auto& owner : secondOwners) require(second->ViewSlot(Abi::FirstStorageImageBinding, &owner, view).has_value(), "a view takes one of the second heap's eight capped slots");
    require(BindlessHeap::NearlyFull(), "two nearly full heaps mark the heap nearly full");
    for (auto& owner : secondOwners) second->ReleaseSlots(&owner);
    require(BindlessHeap::NearlyFull(), "room in the second heap cleared the nearly full state while the first stays nearly full");
    second.reset();
    require(BindlessHeap::NearlyFull(), "destroying the second heap cleared the nearly full state while the first stays nearly full");
    for (auto& owner : firstOwners) first->ReleaseSlots(&owner);
    require(!BindlessHeap::NearlyFull(), "room in the last nearly full heap did not clear the nearly full state");
    first.reset();
}

bool slotPressureSubmits(const Context& context, VkImageView view) {
    auto& heap = BindlessHeap::For(context);
    const auto kind = Abi::FirstImageBinding;
    Recorder recorder(context, true);
    if (!recorder.HasTimeline()) return false;
    recorder.Activate();
    const auto submit = [&] {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        static_cast<void>(recorder.Commands());
        recorder.Submit();
    };
    std::vector<std::shared_ptr<Texture>> held;
    std::vector<std::uint32_t> slots;
    for (std::size_t index = 0; index < 7u; ++index) {
        held.push_back(fakeTexture(index));
        const auto slot = heap.TextureSlot(kind, held.back().get(), view, VK_IMAGE_LAYOUT_GENERAL);
        require(slot.has_value(), "a texture takes one of the eight capped slots");
        slots.push_back(*slot);
    }
    require(BindlessHeap::NearlyFull(), "one free slot of eight marks the heap nearly full");
    submit();
    require(heap.Statistics().slotsEvicted == 0u && BindlessHeap::NearlyFull(),
            "a submission in the middle of a draw evicts no slot the draw took in the batch it submitted");
    const auto submitsBefore = SlotPressureSubmits();
    const auto generation = heap.SlotGeneration(slots[0]);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        static_cast<void>(recorder.Commands());
        require(heap.TakeSlotPressure() && SlotPressureSubmits() == submitsBefore + 1u, "a nearly full heap asks the next draw to submit its batch");
        recorder.Submit();
    }
    require(heap.Statistics().slotsEvicted == slots.size(), "the pressure submission evicts every slot, since no draw is being prepared at draw entry");
    require(!heap.TakeSlotPressure() && !BindlessHeap::NearlyFull(), "slots the submission covers clear the nearly full state");
    require(heap.SlotGeneration(slots[0]) != generation && heap.SlotTexture(kind, slots[0]) == nullptr, "an evicted slot moves its generation and names no texture");
    for (std::size_t index = 7; index < 12u; ++index) {
        held.push_back(fakeTexture(index));
        const auto slot = heap.TextureSlot(kind, held.back().get(), view, VK_IMAGE_LAYOUT_GENERAL);
        require(slot.has_value(), "a new texture takes a slot after the pressure submission, waiting for it when none is free");
    }
    const auto back = heap.TextureSlot(kind, held[0].get(), view, VK_IMAGE_LAYOUT_GENERAL);
    require(back.has_value() && heap.SlotTexture(kind, *back) == held[0].get(), "an evicted texture takes a slot again");
    for (const auto& texture : held) heap.ReleaseSlots(texture.get());
    BindlessHeap::Release(context.device);
    return true;
}

void noTimelineFails(const Context& context) {
    static_cast<void>(BindlessHeap::For(context));
    Recorder recorder(context);
    recorder.Activate();
    bool failed = false;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        static_cast<void>(recorder.Commands());
        try {
            recorder.Submit();
        } catch (const std::runtime_error& error) {
            failed = std::string(error.what()).find("without a timeline semaphore") != std::string::npos;
        }
    }
    require(failed, "a heap batch from a recorder without a timeline, which could not wait for its slots, was accepted");
    BindlessHeap::Release(context.device);
}

bool samplersEvicted(const Context& context) {
    auto& heap = BindlessHeap::For(context);
    Recorder recorder(context, true);
    if (!recorder.HasTimeline()) return false;
    recorder.Activate();
    const auto submit = [&] {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        static_cast<void>(recorder.Commands());
        recorder.Submit();
    };
    SamplerCache cache(Abi::GpuSamplerHeapSlots);
    const auto sampler = [&](std::uint32_t bias) {
        const std::array<std::uint32_t, 4> words{0u, 0u, bias, 0u};
        return cache.Get(context, words, false);
    };
    constexpr std::uint32_t Taken = Abi::GpuSamplerHeapSlots - Abi::GpuSamplerHeapSlots / 4u + 8u;
    const auto pinnedSampler = sampler(Taken);
    const auto pinned = heap.PinnedSamplerSlot(pinnedSampler);
    require(pinned.has_value(), "a pinned sampler takes a slot");
    const auto pinnedGeneration = heap.SamplerGeneration(*pinned);
    std::vector<std::shared_ptr<Sampler>> held;
    std::vector<std::uint32_t> slots;
    for (std::uint32_t index = 0; index < Taken; ++index) {
        held.push_back(sampler(index));
        const auto slot = heap.SamplerSlot(held.back());
        require(slot.has_value(), "a sampler takes a slot");
        require(*slot > *pinned, "the pinned sampler holds a lower slot than every later sampler");
        slots.push_back(*slot);
    }
    require(BindlessHeap::NearlyFull(), "too few free sampler slots mark the heap nearly full");
    const auto generation = heap.SamplerGeneration(slots[0]);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        static_cast<void>(recorder.Commands());
        require(heap.TakeSlotPressure(), "a heap short of sampler slots asks the next draw to submit its batch");
        recorder.Submit();
    }
    require(heap.Statistics().samplersEvicted != 0u, "the pressure submission evicts samplers the new batch has not used");
    require(!BindlessHeap::NearlyFull(), "evicted sampler slots the submission covers clear the nearly full state");
    require(heap.SamplerGeneration(*pinned) == pinnedGeneration && heap.SamplerSlot(pinnedSampler) == pinned, "a pinned sampler slot is never evicted");
    require(heap.SamplerGeneration(slots[0]) != generation, "the oldest sampler's slot was evicted and moved its generation");
    require(recorder.WaitSerialFor(recorder.Submissions(), ~std::uint64_t{0}), "the submission completes");
    const auto back = heap.SamplerSlot(held[0]);
    require(back.has_value(), "an evicted sampler takes a slot again");
    held.clear();
    BindlessHeap::Release(context.device);
    return true;
}

}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    const bool comesBack = !mode.empty();
    try {
        SetTestEnvironment("APS5_GPU_HEAP", "1");
        SetTestEnvironment("APS5_HEAP_SLOT_CAP", comesBack ? "8" : "1");
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->graphicsContext().descriptorHeap) {
            std::printf("skipped, the device has no VK_EXT_descriptor_heap\n");
            return VulkanTestSkipped;
        }
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
                else if (mode == "no-timeline-fails") noTimelineFails(context);
                else if (mode == "idle-slots-evicted" || mode == "pressure-submit" || mode == "samplers-evicted") {
                    const bool ran = mode == "idle-slots-evicted" ? idleSlotsEvicted(context, sampled)
                                     : mode == "pressure-submit" ? slotPressureSubmits(context, sampled)
                                                                 : samplersEvicted(context);
                    if (!ran) {
                        std::printf("skipped, the recorder has no timeline\n");
                        return VulkanTestSkipped;
                    }
                }
                else throw std::runtime_error("unknown mode " + mode);
                device->WaitIdle();
            }
            device.reset();
            std::cout << "the nearly full heap recovers once released slots leave room (" << mode << ")\n";
            return 0;
        }
        {
            const auto context = device->graphicsContext();
            const PlaceholderImages placeholders(context);
            textureSlots(context, placeholders.View(Abi::FirstImageBinding));
            samplerSlots(context);
            if (!dataPool(context)) std::printf("the data pool tests need the recorder timeline\n");
            spanScope(context);
            writeTails(context);
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
