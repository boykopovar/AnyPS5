#include "prx/libSceAgcDriver/Graphics/include/DeviceImageTable.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "DeviceImageTableAbi.hpp"
#include "ImageTableAbi.hpp"
#include "RuntimeAbi.hpp"
#include "Recompiler.hpp"
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
#include <chrono>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
namespace Abi = ShaderRecompiler::DeviceImageTableAbi;
namespace Runtime = ShaderRecompiler::RuntimeAbi;
using Key = DeviceImageTable::Key;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::uint32_t Side = 4;
constexpr std::uint32_t Type2D = 9;
constexpr std::array<std::uint32_t, 5> Formats{1u, 13u, 14u, 22u, 56u};
constexpr std::uint64_t InterfaceId = 0x1234'5678'9abc'def0ull;
constexpr ShaderRecompiler::PipelineIdentity Program{0x5151'0000'0000'0001ull, 0x5151'0000'0000'0002ull};
constexpr std::uint64_t ProgramAddress = 0x7700'0000ull;

alignas(256) std::array<std::array<std::uint32_t, Side * Side>, Formats.size()> Textures{};

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

std::shared_ptr<const DeviceImageTable::Interface> SampledInterface() {
    static const auto sampled = [] {
        auto made = std::make_shared<DeviceImageTable::Interface>();
        made->id = InterfaceId;
        made->image.resourceClass = ShaderRecompiler::ImageResourceClass::Sampled;
        made->image.numericClass = ShaderRecompiler::IrTextureNumericClass::Float;
        made->image.dimension = ShaderRecompiler::RdnaImageDimension::Dim2D;
        made->image.read = true;
        made->image.table = 0;
        made->modes = ShaderRecompiler::ResourceMaterializer::RuntimeImageModes(made->image);
        return made;
    }();
    return sampled;
}

DeviceImageTable::InterfaceLookup Lookup() {
    return [](std::uint64_t id) { return id == InterfaceId ? SampledInterface() : ShaderRecompiler::ResourceMaterializer::FindDeviceImageInterface(id); };
}

struct FakeHeap {
    std::uint32_t cap = Runtime::GpuImageHeapSlots;
    bool nearlyFull = false;
    std::uint64_t serial = 1;
    std::map<std::tuple<std::uint32_t, const void*, VkImageView>, std::uint32_t> slots;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::weak_ptr<Texture>> holders;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> generations;
    std::map<std::uint32_t, std::uint32_t> next;

    void Sweep(std::uint32_t kind, std::uint32_t slot) {
        for (auto found = slots.begin(); found != slots.end();) {
            if (std::get<0>(found->first) == kind && found->second == slot) found = slots.erase(found);
            else ++found;
        }
        holders.erase({kind, slot});
        ++generations[{kind, slot}];
        ++serial;
    }
};

DeviceImageTable::Slots FakeSlots(const std::shared_ptr<FakeHeap>& heap) {
    DeviceImageTable::Slots slots;
    slots.take = [heap](std::uint32_t kind, const std::shared_ptr<Texture>& texture, VkImageView view, VkImageLayout) -> std::optional<std::uint32_t> {
        const auto key = std::make_tuple(kind, static_cast<const void*>(texture.get()), view);
        if (const auto found = heap->slots.find(key); found != heap->slots.end()) return found->second;
        auto& next = heap->next[kind];
        if (next >= heap->cap) return std::nullopt;
        const auto slot = next++;
        heap->slots.emplace(key, slot);
        heap->holders[{kind, slot}] = texture;
        return slot;
    };
    slots.generation = [heap](std::uint32_t kind, std::uint32_t slot) { return heap->generations[{kind, slot}]; };
    slots.serial = [heap] { return heap->serial; };
    slots.nearlyFull = [heap] { return heap->nearlyFull; };
    return slots;
}

Key KeyFor(const std::array<std::uint32_t, 8>& descriptor, std::uint32_t samplerClass = 0u) {
    Key key{};
    std::copy(descriptor.begin(), descriptor.end(), key.begin());
    key[Abi::InterfaceWord] = static_cast<std::uint32_t>(InterfaceId);
    key[Abi::InterfaceWord + 1u] = static_cast<std::uint32_t>(InterfaceId >> 32u);
    key[Abi::SamplerClassWord] = samplerClass;
    return key;
}

void WriteRecords(std::span<std::uint32_t> ring, std::span<const Key> keys, std::uint32_t count, std::uint32_t pcFlags = 0u) {
    ring[Abi::RingCountWord] = count;
    for (std::uint32_t record = 0; record < std::min(count, DeviceImageTable::RingCapacity); ++record) {
        auto* at = ring.data() + Abi::RingRecordsWord + static_cast<std::size_t>(record) * Abi::RecordWords;
        const auto& key = keys[record % keys.size()];
        std::copy(key.begin(), key.end(), at);
        at[Abi::RecordPcWord] = (0x100u + record * 4u) | pcFlags;
    }
}

std::string HexWords(std::span<const std::uint32_t> words) {
    std::ostringstream text;
    text << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < words.size(); ++index) text << (index == 0 ? "" : " ") << std::setw(8) << words[index];
    return text.str();
}

std::vector<std::uint32_t> ReadTable(const Context& context, Recorder& recorder, const DeviceImageTable& table, std::size_t words) {
    Buffer readback(context, words * sizeof(std::uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto commands = recorder.Commands();
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    const VkBufferCopy region{0, 0, words * sizeof(std::uint32_t)};
    context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer")(commands, table.TableBuffer(), readback.Handle(), 1, &region);
    recorder.Sync();
    std::vector<std::uint32_t> result(words);
    std::memcpy(result.data(), readback.Bytes().data(), words * sizeof(std::uint32_t));
    return result;
}

std::vector<Key> FormatKeys() {
    std::vector<Key> keys;
    for (std::size_t index = 0; index < Formats.size(); ++index) keys.push_back(KeyFor(TextureDescriptor(Textures[index].data(), Formats[index])));
    return keys;
}

ShaderRecompiler::RuntimeImageModeMatch ExpectedMode(const Key& key) {
    ShaderRecompiler::DescriptorValue descriptor;
    descriptor.dwordCount = Abi::DescriptorWords;
    descriptor.availableDwordMask = 0xffu;
    std::copy_n(key.begin(), Abi::DescriptorWords, descriptor.dwords.begin());
    const auto sampled = SampledInterface();
    return ShaderRecompiler::ResourceMaterializer::TryRuntimeImageMode(sampled->image, descriptor, sampled->modes);
}

std::uint32_t KindOf(const DeviceImageValue&) {
    return Runtime::FirstImageBinding;
}

void HeaderAtCreation(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "the device has an active recorder");
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    const auto words = ReadTable(context, *recorder, table, DeviceImageTable::TableWords);
    const auto header = std::span<const std::uint32_t>(words).first(Abi::HeaderWords);
    Require(words[Abi::HeaderVersion] == Abi::Version && words[Abi::HeaderMask] == table.Totals().buckets - 1u && words[Abi::HeaderRingCapacity] == DeviceImageTable::RingCapacity, "a new table's header names the version, the bucket mask and the ring capacity before its first prepare (" + HexWords(header) + ")");
    Require(std::all_of(words.begin() + Abi::HeaderRingCapacity + 1u, words.end(), [](std::uint32_t word) { return word == 0u; }), "a new table's other header words and all its buckets are zero");
    table.Prepare(context, *recorder);
    Require(table.Totals().uploadBytes == 0u, "a first prepare with nothing resolved writes no header again (" + std::to_string(table.Totals().uploadBytes) + " bytes)");
    recorder->Sync();
}

void ResolvesAndUploads(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "the device has an active recorder");
    const auto context = device.GraphicsContext();
    const auto heap = std::make_shared<FakeHeap>();
    DeviceImageTable table(context, Lookup(), FakeSlots(heap));
    const auto keys = FormatKeys();
    table.Prepare(context, *recorder);
    WriteRecords(table.RingChunk(*recorder), keys, 8u);
    recorder->Sync();
    auto counts = table.Totals();
    Require(counts.drained == 8u && counts.dropped == 0u, "the batch's eight ring records are drained when it completes");
    Require(counts.inserted == 0u, "nothing is inserted before the next prepare");
    table.Prepare(context, *recorder);
    counts = table.Totals();
    Require(counts.distinct == keys.size() && counts.inserted == keys.size() && counts.entries == keys.size(), "the five distinct records are resolved and inserted (distinct " + std::to_string(counts.distinct) + ", inserted " + std::to_string(counts.inserted) + ")");
    std::set<std::pair<std::uint32_t, std::uint32_t>> slots;
    for (std::size_t index = 0; index < keys.size(); ++index) {
        const auto value = table.Find(keys[index]);
        Require(value.has_value() && value->fault == 0u && value->samplerSlot == 0u, "format " + std::to_string(Formats[index]) + " is inserted without a fault");
        const auto expected = ExpectedMode(keys[index]);
        Require(expected.mode.has_value() && value->mode == *expected.mode, "format " + std::to_string(Formats[index]) + " names the mode the CPU table path picks");
        const auto kind = KindOf(*value);
        const auto holder = heap->holders.find({kind, value->slot});
        Require(holder != heap->holders.end() && holder->second.lock() == table.EntryTexture(keys[index]), "format " + std::to_string(Formats[index]) + " holds its entry's texture in its heap slot");
        Require(slots.insert({kind, value->slot}).second, "every format has its own heap slot");
    }
    const auto words = table.KeyWords();
    const auto uploaded = ReadTable(context, *recorder, table, Abi::HeaderWords + words.size());
    Require(uploaded[Abi::HeaderVersion] == Abi::Version && uploaded[Abi::HeaderMask] == counts.buckets - 1u && uploaded[Abi::HeaderRingCapacity] == DeviceImageTable::RingCapacity, "the header names the version, the bucket mask and the ring capacity");
    Require(std::all_of(uploaded.begin() + Abi::HeaderRingCapacity + 1u, uploaded.begin() + Abi::HeaderWords, [](std::uint32_t word) { return word == 0u; }), "the header's other words stay zero");
    Require(std::equal(words.begin(), words.end(), uploaded.begin() + Abi::HeaderWords), "the device's buckets equal the host key table");
    const auto uploads = table.Totals().uploads;
    table.Prepare(context, *recorder);
    Require(table.Totals().uploads == uploads, "a prepare with nothing changed uploads nothing");
    recorder->Sync();
    std::printf("resolved %zu formats: %llu uploads, %llu bytes\n", keys.size(), static_cast<unsigned long long>(table.Totals().uploads), static_cast<unsigned long long>(table.Totals().uploadBytes));
}

void TrailerAddresses(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    static_cast<void>(recorder->Commands());
    const auto first = table.Trailer(*recorder);
    Require(first.buckets != 0u && first.ring != 0u, "the trailer names a bucket address and a ring chunk address");
    Require(table.Trailer(*recorder) == first, "one batch reads one ring chunk");
    recorder->Submit();
    static_cast<void>(recorder->Commands());
    const auto second = table.Trailer(*recorder);
    Require(second.buckets == first.buckets, "every batch reads the same buckets");
    const auto stride = second.ring > first.ring ? second.ring - first.ring : first.ring - second.ring;
    Require(stride >= static_cast<std::uint64_t>(DeviceImageTable::RingWords) * sizeof(std::uint32_t) && stride % context.limits.minStorageBufferOffsetAlignment == 0u, "a batch still in flight keeps its chunk, so the next batch reads another one, a whole aligned chunk away");
    recorder->Sync();
    std::printf("trailer: buckets 0x%llx, ring chunks 0x%llx and 0x%llx\n", static_cast<unsigned long long>(first.buckets), static_cast<unsigned long long>(first.ring), static_cast<unsigned long long>(second.ring));
}

void SweptSlotsLeave(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    const auto heap = std::make_shared<FakeHeap>();
    DeviceImageTable table(context, Lookup(), FakeSlots(heap));
    const auto keys = FormatKeys();
    table.Prepare(context, *recorder);
    WriteRecords(table.RingChunk(*recorder), keys, static_cast<std::uint32_t>(keys.size()));
    recorder->Sync();
    table.Prepare(context, *recorder);
    Require(table.Totals().entries == keys.size(), "every format is inserted");
    ++heap->serial;
    table.Prepare(context, *recorder);
    Require(table.Totals().entries == keys.size() && table.Totals().swept == 0u, "a heap whose slots moved elsewhere leaves the table whole");
    const auto swept = table.Find(keys[2]);
    Require(swept.has_value(), "the swept key was inserted");
    const auto texture = table.EntryTexture(keys[2]);
    heap->Sweep(KindOf(*swept), swept->slot);
    const auto uploads = table.Totals().uploads;
    table.Prepare(context, *recorder);
    const auto counts = table.Totals();
    Require(!table.Find(keys[2]).has_value() && counts.swept == 1u && counts.entries == keys.size() - 1u, "a key whose heap slot was swept leaves the table (" + std::to_string(counts.swept) + " swept, " + std::to_string(counts.entries) + " left)");
    Require(counts.uploads == uploads + 1u, "the table without the swept key is uploaded before the batch");
    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 2u) Require(table.Find(keys[index]).has_value(), "a key whose slot was not swept stays");
    }
    Require(table.EntryTexture(keys[2]) == nullptr && texture != nullptr && texture.use_count() >= 2, "the swept entry lets go of its texture, which the open batch keeps");
    WriteRecords(table.RingChunk(*recorder), std::span<const Key>(keys).subspan(2, 1), 1u);
    recorder->Sync();
    table.Prepare(context, *recorder);
    const auto again = table.Find(keys[2]);
    Require(again.has_value() && again->fault == 0u && table.Totals().inserted == keys.size() + 1u, "the swept key's next miss inserts it again with a fresh slot");
    recorder->Sync();
}

void MarginRefuses(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    const auto heap = std::make_shared<FakeHeap>();
    heap->nearlyFull = true;
    DeviceImageTable table(context, Lookup(), FakeSlots(heap));
    table.NoteProgram(Program, ProgramAddress);
    const std::array<ShaderRecompiler::PipelineIdentity, 1> programs{Program};
    const auto keys = FormatKeys();
    table.Prepare(context, *recorder, programs);
    WriteRecords(table.RingChunk(*recorder), keys, static_cast<std::uint32_t>(keys.size()));
    recorder->Sync();
    table.Prepare(context, *recorder, programs);
    const auto counts = table.Totals();
    Require(counts.inserted == 0u && counts.heapFull == keys.size() && heap->slots.empty(), "a heap inside its margin gives the table no slot (" + std::to_string(counts.heapFull) + " refused)");
    Require(!table.ServesProgram(ProgramAddress) && counts.fallbacks == 1u, "the program whose keys found no slot falls back to the CPU path");
    Require(table.LastFallback().find("outside its margin") != std::string::npos, "the fallback names the margin: " + table.LastFallback());
    recorder->Sync();
}

void RejectsAndFaults(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    auto invalid = TextureDescriptor(Textures[0].data(), Formats[0]);
    invalid[1] &= ~(0x1ffu << 20u);
    auto unmapped = TextureDescriptor(reinterpret_cast<const void*>(std::uintptr_t{0x10000}), Formats[4]);
    const std::array<Key, 2> keys{KeyFor(invalid), KeyFor(unmapped)};
    const auto checked = ExpectedMode(keys[0]);
    Require(!checked.mode.has_value(), "the CPU table path rejects a descriptor of format 0");
    table.Prepare(context, *recorder);
    WriteRecords(table.RingChunk(*recorder), keys, 2u);
    recorder->Sync();
    table.Prepare(context, *recorder);
    const auto first = table.Find(keys[0]);
    Require(first.has_value() && first->fault == 1u + static_cast<std::uint32_t>(checked.reason), "a descriptor the mode check rejects is inserted with 1 + its reason");
    const auto second = table.Find(keys[1]);
    Require(second.has_value() && second->fault == 1u + static_cast<std::uint32_t>(ShaderRecompiler::ImageTableAbi::PoisonReason::DriverRejected), "a descriptor over unmapped memory is inserted as rejected by the driver");
    Require(table.Totals().rejected == 2u && table.Totals().inserted == 0u, "both are rejections, neither holds a texture");
    WriteRecords(table.RingChunk(*recorder), std::span<const Key>(keys).first(1), 1u, Abi::FaultFlag);
    recorder->Sync();
    const auto fault = table.LastFault();
    const auto name = ShaderRecompiler::ImageTableAbi::PoisonReasonName(checked.reason);
    Require(table.Totals().faults == 1u, "the fault record is counted");
    Require(fault.find("image table: pc 0x100 selected the descriptor (words " + HexWords(std::span<const std::uint32_t>(invalid)) + ")") != std::string::npos && fault.find(std::string(name)) != std::string::npos, "the fault names the pc, the descriptor's words and the reason: " + fault);
    std::printf("fault: %s\n", fault.c_str());
}

constexpr std::size_t BlockBytes = std::size_t{4} << 20u;
constexpr std::size_t TextureStride = 1024u;
constexpr std::uint32_t WatchedTextures = 2000u;

class GuestBlock {
public:
    explicit GuestBlock(bool watched = true) : watched(watched) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (watched) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        if (watched) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    std::uint8_t* Texture(std::uint32_t index) const { return block + static_cast<std::size_t>(index) * TextureStride; }

private:
    bool watched = true;
    std::uint8_t* block = nullptr;
};

double Microseconds(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
}

void RefreshesWrittenEntries(AgcDriver::VulkanDevice& device) {
    GuestBlock block;
    for (std::uint32_t index = 0; index < WatchedTextures; ++index) std::memset(block.Texture(index), static_cast<int>(index & 0xffu), Side * Side * 4u);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    AgcDriver::GuestMemory::BumpCollectEpoch();
    if (!AgcDriver::GuestMemory::WriteWatched()) {
        std::puts("guest memory has no write watch: device image entry refreshes are not tested");
        return;
    }
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    std::vector<Key> keys;
    for (std::uint32_t index = 0; index < WatchedTextures; ++index) keys.push_back(KeyFor(TextureDescriptor(block.Texture(index), Formats[4])));
    table.Prepare(context, *recorder);
    WriteRecords(table.RingChunk(*recorder), keys, WatchedTextures);
    recorder->Sync();
    auto start = std::chrono::steady_clock::now();
    table.Prepare(context, *recorder);
    const auto insertUs = Microseconds(start);
    auto counts = table.Totals();
    Require(counts.inserted == WatchedTextures && counts.entries == WatchedTextures, "the 2000 entries are inserted (" + std::to_string(counts.inserted) + ")");
    const auto insertBytes = counts.uploadBytes;
    recorder->Sync();
    table.Prepare(context, *recorder);
    start = std::chrono::steady_clock::now();
    constexpr int Prepares = 100;
    for (int round = 0; round < Prepares; ++round) table.Prepare(context, *recorder);
    const auto sameEpochUs = Microseconds(start) / Prepares;
    Require(table.Totals().refreshed == 0u, "nothing is refreshed while nothing was written");
    double newEpochUs = 0;
    for (int round = 0; round < Prepares; ++round) {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        start = std::chrono::steady_clock::now();
        table.Prepare(context, *recorder);
        newEpochUs += Microseconds(start);
    }
    newEpochUs /= Prepares;
    counts = table.Totals();
    Require(counts.refreshed == 0u, "a new collect epoch with nothing written refreshes nothing (" + std::to_string(counts.refreshed) + ")");
    const auto uploads = counts.uploads;
    const auto written = block.Texture(777u);
    std::memset(written, 0x5a, Side * Side * 4u);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    start = std::chrono::steady_clock::now();
    table.Prepare(context, *recorder);
    const auto refreshUs = Microseconds(start);
    counts = table.Totals();
    Require(counts.refreshed == 1u, "a CPU write to one texture refreshes exactly its entry (" + std::to_string(counts.refreshed) + " refreshed)");
    const auto value = table.Find(keys[777]);
    Require(value.has_value() && value->fault == 0u, "the refreshed entry stays in the table");
    recorder->Sync();
    std::printf("2000 entries: insert %.1f us per key (%.1f bytes uploaded per insert); prepare with nothing changed %.2f us in the same epoch, %.1f us in a new epoch (%llu scans); one written texture refreshed in %.1f us (%llu entries of its 64 KiB blocks checked), %llu uploads after it\n",
        insertUs / WatchedTextures, static_cast<double>(insertBytes) / WatchedTextures, sameEpochUs, newEpochUs, static_cast<unsigned long long>(counts.scans), refreshUs, static_cast<unsigned long long>(counts.checked), static_cast<unsigned long long>(counts.uploads - uploads));
}

void RefreshesOnOwnStores(AgcDriver::VulkanDevice& device) {
    GuestBlock block;
    constexpr std::uint32_t Count = 16u;
    for (std::uint32_t index = 0; index < Count; ++index) std::memset(block.Texture(index), static_cast<int>(index + 1u), Side * Side * 4u);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    AgcDriver::GuestMemory::BumpCollectEpoch();
    if (!AgcDriver::GuestMemory::WriteWatched()) {
        std::puts("guest memory has no write watch: refreshes after the driver's own stores are not tested");
        return;
    }
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    std::vector<Key> keys;
    for (std::uint32_t index = 0; index < Count; ++index) keys.push_back(KeyFor(TextureDescriptor(block.Texture(index), Formats[4])));
    table.Prepare(context, *recorder);
    WriteRecords(table.RingChunk(*recorder), keys, Count);
    recorder->Sync();
    table.Prepare(context, *recorder);
    table.Prepare(context, *recorder);
    Require(table.Totals().entries == Count && table.Totals().refreshed == 0u, "the entries are inserted and nothing is refreshed");
    const auto before = table.EntryTexture(keys[5]);
    const auto address = reinterpret_cast<std::uint64_t>(block.Texture(5));
    std::memset(block.Texture(5), 0x77, Side * Side * 4u);
    static_cast<void>(AgcDriver::GuestMemory::MarkWritten(address, Side * Side * 4u));
    table.Prepare(context, *recorder);
    const auto counts = table.Totals();
    Require(counts.refreshed == 1u && table.EntryTexture(keys[5]) != before, "a store the driver made in the same collect epoch refreshes the entry it covers (" + std::to_string(counts.refreshed) + " refreshed)");
    recorder->Sync();
}

void KeepsUnwatchedEntries(AgcDriver::VulkanDevice& device) {
    GuestBlock block(false);
    constexpr std::uint32_t Count = 16u;
    for (std::uint32_t index = 0; index < Count; ++index) std::memset(block.Texture(index), static_cast<int>(0x40u + index), Side * Side * 4u);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    std::vector<Key> keys;
    for (std::uint32_t index = 0; index < Count; ++index) keys.push_back(KeyFor(TextureDescriptor(block.Texture(index), Formats[4])));
    table.Prepare(context, *recorder);
    WriteRecords(table.RingChunk(*recorder), keys, Count);
    recorder->Sync();
    table.Prepare(context, *recorder);
    Require(table.Totals().entries == Count, "the unwatched entries are inserted");
    AgcDriver::GuestMemory::BumpCollectEpoch();
    table.Prepare(context, *recorder);
    const auto settled = table.Totals();
    for (int round = 0; round < 4; ++round) {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        table.Prepare(context, *recorder);
    }
    auto counts = table.Totals();
    Require(counts.checked == settled.checked && counts.refreshed == 0u, "a new collect epoch over unwatched entries whose bytes still match looks none of them up again (" + std::to_string(counts.checked - settled.checked) + " checked)");
    const auto before = table.EntryTexture(keys[9]);
    std::memset(block.Texture(9), 0x99, Side * Side * 4u);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    table.Prepare(context, *recorder);
    counts = table.Totals();
    Require(counts.refreshed == 1u && table.EntryTexture(keys[9]) != before, "a CPU write to one unwatched texture refreshes its entry at the next collect epoch (" + std::to_string(counts.refreshed) + " refreshed)");
    std::printf("unwatched entries: %llu checked over 5 epochs with nothing written, 1 refreshed after a CPU write\n", static_cast<unsigned long long>(settled.checked));
    recorder->Sync();
}

void EvictsLeastRecentlyUsed(AgcDriver::VulkanDevice& device) {
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()), DeviceImageTable::Budget{4u, std::uint64_t{1} << 40u, 0u, 0u});
    const auto keys = FormatKeys();
    table.Prepare(context, *recorder);
    WriteRecords(table.RingChunk(*recorder), std::span<const Key>(keys).first(4), 4u);
    recorder->Sync();
    table.Prepare(context, *recorder);
    Require(table.Totals().entries == 4u && table.Totals().evicted == 0u, "four entries fit the budget");
    auto oldest = table.EntryTexture(keys[0]);
    Require(oldest != nullptr, "the oldest entry holds its texture");
    const auto held = oldest.use_count();
    WriteRecords(table.RingChunk(*recorder), std::span<const Key>(keys).subspan(4), 1u);
    recorder->Sync();
    table.Prepare(context, *recorder);
    const auto counts = table.Totals();
    Require(counts.entries == 3u && counts.evicted == 2u, "a fifth entry over a budget of four evicts down to three (" + std::to_string(counts.entries) + " left, " + std::to_string(counts.evicted) + " evicted)");
    Require(!table.Find(keys[0]).has_value() && !table.Find(keys[1]).has_value(), "the two least recently used entries are evicted");
    Require(table.Find(keys[2]).has_value() && table.Find(keys[3]).has_value() && table.Find(keys[4]).has_value(), "the recently used entries stay");
    Require(oldest.use_count() == held, "an evicted entry's texture is kept by the open batch");
    recorder->Sync();
    recorder->FlushDeferredReleases();
    gpu.unlock();
    for (int wait = 0; wait < 5000 && oldest.use_count() >= held; ++wait) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(oldest.use_count() == held - 1, "the evicted texture is released once its batch completed (" + std::to_string(oldest.use_count()) + " of " + std::to_string(held) + ")");
}

void CountsOverflow(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    const auto keys = FormatKeys();
    table.Prepare(context, *recorder);
    WriteRecords(table.RingChunk(*recorder), keys, DeviceImageTable::RingCapacity + 100u);
    recorder->Sync();
    const auto counts = table.Totals();
    Require(counts.drained == DeviceImageTable::RingCapacity && counts.dropped == 100u, "records past the ring's capacity are counted as dropped (drained " + std::to_string(counts.drained) + ", dropped " + std::to_string(counts.dropped) + ")");
    table.Prepare(context, *recorder);
    Require(table.Totals().inserted == keys.size(), "the records that fit are still resolved");
    recorder->Sync();
}

void SettlesUnexpectedBatches(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    table.NoteProgram(Program, ProgramAddress);
    const std::array<ShaderRecompiler::PipelineIdentity, 1> programs{Program};
    const auto keys = FormatKeys();
    table.Prepare(context, *recorder, programs);
    Require(table.UnexpectedSerial(*recorder) == 0u, "a batch whose device images were all expected gives a present nothing to wait for");
    recorder->Sync();
    table.Prepare(context, *recorder, programs);
    table.NoteUnexpectedBatch(*recorder);
    table.NoteUnexpectedBatch(*recorder);
    WriteRecords(table.RingChunk(*recorder), keys, static_cast<std::uint32_t>(keys.size()));
    const auto serial = table.UnexpectedSerial(*recorder);
    Require(serial != 0u && serial == recorder->Submissions(), "a present waits for the submitted batch that holds a program with unexpected keys (serial " + std::to_string(serial) + ", submitted " + std::to_string(recorder->Submissions()) + ")");
    if (recorder->HasTimeline()) {
        recorder->WaitSerial(serial);
        recorder->FinishUpTo(serial);
    } else {
        recorder->Sync();
    }
    auto counts = table.Totals();
    Require(counts.drained == keys.size() && counts.inserted == 0u, "the waited batch's misses are drained but not yet resolved (drained " + std::to_string(counts.drained) + ", inserted " + std::to_string(counts.inserted) + ")");
    table.Settle(context, *recorder);
    counts = table.Totals();
    Require(counts.inserted == keys.size() && counts.unexpectedBatches == 1u && counts.settles == 1u, "settling resolves every missed key before the next batch (inserted " + std::to_string(counts.inserted) + ", unexpected batches " + std::to_string(counts.unexpectedBatches) + ", settles " + std::to_string(counts.settles) + ")");
    for (const auto& key : keys) Require(table.Find(key).has_value(), "a key missed by a program with unexpected keys is in the table after the present settled it");
    Require(table.UnexpectedSerial(*recorder) == 0u, "a settled batch gives the next present nothing to wait for");
    recorder->Sync();
    std::printf("a present settles the batch of a program with unexpected keys: %zu keys in the table before the next batch\n", keys.size());
}

void OverflowFallsBack(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    table.NoteProgram(Program, ProgramAddress);
    const std::array<ShaderRecompiler::PipelineIdentity, 1> programs{Program};
    const auto keys = FormatKeys();
    table.Prepare(context, *recorder, programs);
    WriteRecords(table.RingChunk(*recorder), keys, static_cast<std::uint32_t>(keys.size()));
    recorder->Sync();
    for (std::uint32_t batch = 1; batch <= DeviceImageTable::OverflowBatches; ++batch) {
        Require(table.ServesProgram(ProgramAddress), "the program keeps the device path before " + std::to_string(DeviceImageTable::OverflowBatches) + " overflowing batches");
        table.Prepare(context, *recorder, programs);
        WriteRecords(table.RingChunk(*recorder), keys, DeviceImageTable::RingCapacity + 1u);
        recorder->Sync();
    }
    Require(!table.ServesProgram(ProgramAddress) && !table.ServesIdentity(programs[0]) && table.Totals().fallbacks == 1u, "a program whose misses overflow the ring in three batches in a row after the table resolved its misses falls back to the CPU path");
    Require(table.ServesProgram(ProgramAddress + 0x100u), "only that program falls back");
    DeviceImageTable other(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    other.NoteProgram(Program, ProgramAddress);
    constexpr std::uint32_t Overflow = DeviceImageTable::RingCapacity + 1u;
    for (const auto count : {1u, Overflow, Overflow, 1u, Overflow, Overflow}) {
        other.Prepare(context, *recorder, programs);
        WriteRecords(other.RingChunk(*recorder), keys, count);
        recorder->Sync();
    }
    Require(other.ServesProgram(ProgramAddress) && other.OverflowStreak(programs[0]) == 2u, "a batch without overflow between them resets the count (streak " + std::to_string(other.OverflowStreak(programs[0])) + ")");
}

void OverflowsBeforeAResolveCountOnce(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    table.NoteProgram(Program, ProgramAddress);
    const std::array<ShaderRecompiler::PipelineIdentity, 1> programs{Program};
    const auto keys = FormatKeys();
    for (std::uint32_t batch = 0; batch < DeviceImageTable::OverflowBatches + 1u; ++batch) {
        static_cast<void>(recorder->Commands());
        table.Prepare(context, *recorder, programs);
        WriteRecords(table.RingChunk(*recorder), keys, DeviceImageTable::RingCapacity + 1u);
        recorder->Submit();
    }
    Require(table.Totals().drained == 0u, "the overflowing batches are still in flight");
    recorder->Sync();
    const auto drained = table.Totals();
    Require(drained.dropped == DeviceImageTable::OverflowBatches + 1u, "every batch overflowed the ring (dropped " + std::to_string(drained.dropped) + ")");
    Require(table.ServesProgram(ProgramAddress) && drained.fallbacks == 0u, "batches that overflow before the table resolved the misses of the first count as one overflow");
    table.Prepare(context, *recorder, programs);
    Require(table.Totals().inserted == keys.size(), "the records that fit are resolved");
    recorder->Sync();
}

void OverflowsBeforeTheirInsertsDoNotCount(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    table.NoteProgram(Program, ProgramAddress);
    const std::array<ShaderRecompiler::PipelineIdentity, 1> programs{Program};
    const auto keys = FormatKeys();
    std::vector<std::uint64_t> serials;
    for (std::uint32_t batch = 0; batch < DeviceImageTable::OverflowBatches + 2u; ++batch) {
        static_cast<void>(recorder->Commands());
        table.Prepare(context, *recorder, programs);
        WriteRecords(table.RingChunk(*recorder), keys, DeviceImageTable::RingCapacity + 1u);
        serials.push_back(recorder->SubmitAndEpoch());
    }
    Require(table.Totals().drained == 0u, "the overflowing batches are still in flight");
    for (const auto serial : serials) {
        recorder->FinishUpTo(serial);
        static_cast<void>(recorder->Commands());
        table.Prepare(context, *recorder, programs);
    }
    const auto drained = table.Totals();
    Require(drained.dropped == serials.size() && drained.inserted == keys.size(), "every batch overflowed the ring and the first batch's records were resolved before the others drained (dropped " + std::to_string(drained.dropped) + ", inserted " + std::to_string(drained.inserted) + ")");
    const auto streak = table.OverflowStreak(programs[0]);
    Require(table.ServesProgram(ProgramAddress) && drained.fallbacks == 0u && streak == 0u, "batches taken before the inserts that resolved their misses neither count toward the overflow streak nor make the program fall back (streak " + std::to_string(streak) + ", " + std::to_string(drained.fallbacks) + " fallbacks)");
    recorder->Sync();
}

void WarmOverflowFallsBack(AgcDriver::VulkanDevice& device) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()));
    table.NoteProgram(Program, ProgramAddress);
    const std::array<ShaderRecompiler::PipelineIdentity, 1> programs{Program};
    const auto warm = FormatKeys();
    table.Prepare(context, *recorder, programs);
    WriteRecords(table.RingChunk(*recorder), warm, static_cast<std::uint32_t>(warm.size()));
    recorder->Sync();
    std::vector<Key> keys(DeviceImageTable::RingCapacity + 1u, warm[0]);
    for (std::uint32_t batch = 0; batch < DeviceImageTable::OverflowBatches; ++batch) {
        Require(table.ServesProgram(ProgramAddress), "the program keeps the device path before " + std::to_string(DeviceImageTable::OverflowBatches) + " overflowing batches");
        for (std::uint32_t index = 0; index < keys.size(); ++index) keys[index][Abi::SamplerWord] = batch * static_cast<std::uint32_t>(keys.size()) + index + 1u;
        table.Prepare(context, *recorder, programs);
        WriteRecords(table.RingChunk(*recorder), keys, static_cast<std::uint32_t>(keys.size()));
        recorder->Sync();
    }
    const auto counts = table.Totals();
    const auto resolved = warm.size() + (DeviceImageTable::OverflowBatches - 1u) * DeviceImageTable::RingCapacity;
    Require(counts.inserted == resolved && counts.dropped == DeviceImageTable::OverflowBatches, "the table resolved the warm keys and each batch's recorded misses before the next batch was taken (" + std::to_string(counts.inserted) + " inserted, " + std::to_string(counts.dropped) + " dropped)");
    Require(!table.ServesProgram(ProgramAddress) && counts.fallbacks == 1u, "a program that misses more distinct keys than the ring holds in three batches in a row, each taken after the previous batch's misses were inserted, falls back to the CPU path");
    const auto reason = "its misses overflowed the ring of " + std::to_string(DeviceImageTable::RingCapacity) + " records in " + std::to_string(DeviceImageTable::OverflowBatches) + " batches in a row";
    Require(table.LastFallback() == reason, "the fallback gives the overflow reason: " + table.LastFallback());
}

void FullTableFallsBack(AgcDriver::VulkanDevice& device) {
    GuestBlock block;
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    const auto context = device.GraphicsContext();
    DeviceImageTable table(context, Lookup(), FakeSlots(std::make_shared<FakeHeap>()), DeviceImageTable::Budget{1u << 20u, std::uint64_t{1} << 40u, 0u, 0u, 16u});
    table.NoteProgram(Program, ProgramAddress);
    const std::array<ShaderRecompiler::PipelineIdentity, 1> programs{Program};
    std::vector<Key> keys;
    for (std::uint32_t index = 0; index < 12u; ++index) keys.push_back(KeyFor(TextureDescriptor(block.Texture(index), Formats[4])));
    table.Prepare(context, *recorder, programs);
    WriteRecords(table.RingChunk(*recorder), keys, static_cast<std::uint32_t>(keys.size()));
    recorder->Sync();
    table.Prepare(context, *recorder, programs);
    const auto counts = table.Totals();
    Require(counts.buckets == 16u && counts.entries < keys.size() && counts.tableFull != 0u, "a table of at most 16 buckets fills (" + std::to_string(counts.entries) + " entries)");
    Require(!table.ServesProgram(ProgramAddress) && counts.fallbacks == 1u, "a program whose key finds the table full falls back to the CPU path");
    recorder->Sync();
}

alignas(4096) std::array<std::uint8_t, 65536> CubeTexels{};

void NonSeamlessCube(AgcDriver::VulkanDevice& device) {
    constexpr std::uint32_t TypeCube = 11;
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto context = device.GraphicsContext();
    auto cube = TextureDescriptor(CubeTexels.data(), Formats[4]);
    cube[3] = 0xfacu | (TypeCube << 28u);
    cube[4] = 5u;
    const DeviceTableView view{ShaderRecompiler::DescriptorImageShape::Image2DArray, false, false};
    const auto seamless = LookupDeviceTableTexture(context, cube, view, 0u);
    Require(seamless.texture != nullptr && seamless.unservable.empty(), "a cube T# resolves through the texture cache");
    try {
        const auto resolved = LookupDeviceTableTexture(context, cube, view, Abi::SamplerNonSeamlessCube);
        Require(context.nonSeamlessCubeMap && resolved.texture != nullptr, "a non-seamless cube sampler is served only by a device with VK_EXT_non_seamless_cube_map");
    } catch (const std::runtime_error& error) {
        Require(!context.nonSeamlessCubeMap && std::string(error.what()).find("needs VK_EXT_non_seamless_cube_map") != std::string::npos, std::string("a non-seamless cube sampler is rejected only without VK_EXT_non_seamless_cube_map: ") + error.what());
    }
    std::printf("non-seamless cube sampler: %s (VK_EXT_non_seamless_cube_map %s)\n", context.nonSeamlessCubeMap ? "served" : "rejected", context.nonSeamlessCubeMap ? "on" : "off");
}

void RegistersInterfaces() {
    using ShaderRecompiler::ResourceMaterializer;
    auto image = SampledInterface()->image;
    image.source = 7u;
    image.firstUsePc = 0x40u;
    image.table = 3u;
    const auto modes = ResourceMaterializer::RuntimeImageModes(image);
    const auto id = ResourceMaterializer::DeviceImageInterfaceId(image, modes);
    Require(ResourceMaterializer::FindDeviceImageInterface(id) == nullptr, "an interface no program registered is not found");
    Require(ResourceMaterializer::RegisterDeviceImageInterface(image, modes) == id, "registering an interface gives its id");
    const auto found = ResourceMaterializer::FindDeviceImageInterface(id);
    Require(found != nullptr && found->id == id && found->modes.size() == modes.size() && found->image.source == 0u && found->image.table == 0u, "the registry holds the interface without the program's source, pc or table number");
    auto elsewhere = image;
    elsewhere.source = 9u;
    elsewhere.firstUsePc = 0x80u;
    elsewhere.table = 1u;
    Require(ResourceMaterializer::DeviceImageInterfaceId(elsewhere, modes) == id && ResourceMaterializer::RegisterDeviceImageInterface(elsewhere, modes) == id, "the same image read from another source, pc or table shares the interface");
    auto integer = image;
    integer.numericClass = ShaderRecompiler::IrTextureNumericClass::Uint;
    Require(ResourceMaterializer::DeviceImageInterfaceId(integer, ResourceMaterializer::RuntimeImageModes(integer)) != id, "an image of another numeric class has another interface");
}

void DevicePathRefusals() {
    Require(!DeviceImagePathRefusal({true, true}).has_value(), "the GPU heap on a device with VK_EXT_descriptor_heap serves the device image path");
    const auto withoutHeap = DeviceImagePathRefusal({false, true});
    Require(withoutHeap.has_value() && withoutHeap->find("APS5_GPU_HEAP") != std::string::npos, "the device image path stays off without APS5_GPU_HEAP and says so");
    const auto withoutExtension = DeviceImagePathRefusal({true, false});
    Require(withoutExtension.has_value() && withoutExtension->find("VK_EXT_descriptor_heap") != std::string::npos, "the device image path stays off without VK_EXT_descriptor_heap and says so");
    Require(DeviceImagePathRefusal({false, false}).has_value(), "the device image path stays off with neither");
}

int HeapRetention() {
    auto device = OpenVulkanTestDevice();
    if (device == nullptr) return VulkanTestSkipped;
    if (!device->GraphicsContext().descriptorHeap) {
        std::printf("skipped, the device lacks VK_EXT_descriptor_heap, which the GPU heap the device image table takes its slots from needs\n");
        return VulkanTestSkipped;
    }
    {
        const auto context = device->GraphicsContext();
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        auto* recorder = Recorder::Active();
        DeviceImageTable table(context, Lookup(), DeviceImageTable::HeapSlots(context));
        const auto keys = FormatKeys();
        table.Prepare(context, *recorder);
        WriteRecords(table.RingChunk(*recorder), keys, static_cast<std::uint32_t>(keys.size()));
        recorder->Sync();
        table.Prepare(context, *recorder);
        Require(table.Totals().entries == keys.size(), "the heap gives every format a slot");
        auto& heap = BindlessHeap::For(context);
        std::vector<std::pair<std::uint32_t, std::uint32_t>> generations;
        for (const auto& key : keys) {
            const auto value = table.Find(key);
            Require(value.has_value(), "every format is inserted");
            Require(heap.SlotTexture(KindOf(*value), value->slot) == table.EntryTexture(key).get(), "the heap slot holds the entry's texture");
            generations.emplace_back(KindOf(*value), heap.SlotGeneration(value->slot));
        }
        BindlessHeap::NoteSweepDue();
        BindlessHeap::SweepIfDue();
        table.Prepare(context, *recorder);
        for (std::size_t index = 0; index < keys.size(); ++index) {
            const auto value = table.Find(keys[index]);
            Require(value.has_value() && heap.SlotGeneration(value->slot) == generations[index].second, "the heap's sweep keeps the slot of a texture the table holds");
        }
        Require(table.Totals().swept == 0u, "nothing left the table");
        recorder->Sync();
    }
    device->WaitIdle();
    device.reset();
    std::cout << "device image table heap retention tests passed\n";
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "";
        for (std::size_t index = 0; index < Textures.size(); ++index) {
            for (std::size_t texel = 0; texel < Textures[index].size(); ++texel) Textures[index][texel] = static_cast<std::uint32_t>(0x01020304u * (index + 1u) + texel);
        }
        if (mode == "--heap") return HeapRetention();
        RegistersInterfaces();
        DevicePathRefusals();
        auto device = OpenVulkanTestDevice();
        if (device == nullptr) return VulkanTestSkipped;
        HeaderAtCreation(*device);
        ResolvesAndUploads(*device);
        TrailerAddresses(*device);
        SweptSlotsLeave(*device);
        MarginRefuses(*device);
        RejectsAndFaults(*device);
        CountsOverflow(*device);
        SettlesUnexpectedBatches(*device);
        RefreshesWrittenEntries(*device);
        RefreshesOnOwnStores(*device);
        KeepsUnwatchedEntries(*device);
        EvictsLeastRecentlyUsed(*device);
        FullTableFallsBack(*device);
        OverflowFallsBack(*device);
        OverflowsBeforeAResolveCountOnce(*device);
        OverflowsBeforeTheirInsertsDoNotCount(*device);
        WarmOverflowFallsBack(*device);
        NonSeamlessCube(*device);
        device->WaitIdle();
        device.reset();
        std::cout << "device image table tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "device image table tests failed: " << error.what() << '\n';
        return 1;
    }
}
