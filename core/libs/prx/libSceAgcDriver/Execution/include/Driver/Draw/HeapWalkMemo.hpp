#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_HEAPWALKMEMO_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_HEAPWALKMEMO_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

struct HeapWalkEntry {
    std::uint64_t variant = 0;
    std::uint64_t shaderBase = 0;
    std::uint32_t pushOffset = 0;
    std::vector<std::uint32_t> userData;
    std::vector<ShaderRecompiler::TargetMerge> merges;
    std::shared_ptr<const std::vector<std::pair<std::uint64_t, std::uint64_t>>> ranges;
    std::shared_ptr<const std::vector<std::uint32_t>> values;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> accessible;
    ShaderRecompiler::SrtUserDataUse userDataUse;
    std::shared_ptr<const ShaderRecompiler::ResourceCapture> capture;
    std::shared_ptr<const ShaderRecompiler::RecompileResult> result;
    std::shared_ptr<const ShaderRecompiler::HeapStageWords> words;
    std::uint64_t bytes = 0;
    std::uint64_t stored = 0;
};

constexpr std::size_t HeapWalkWays = 4;

constexpr std::size_t HeapWalkMemoKeys = 16384;

constexpr std::size_t HeapWalkSeenKeys = 1u << 16;

constexpr std::size_t HeapWalkVariantSlots = 4096;

constexpr std::uint32_t HeapWalkProbeLookups = 64;

constexpr std::uint32_t HeapWalkSkippedWalks = 1024;

struct HeapWalkVariantUse {
    std::uint64_t variant = 0;
    std::uint32_t lookups = 0;
    std::uint32_t hits = 0;
    std::uint32_t skip = 0;
    std::uint32_t skipped = 0;
    std::uint32_t repeats = 0;
};

std::uint64_t HeapWalkEntryBytes(const HeapWalkEntry& entry);

std::uint64_t HeapWalkEntryOwnBytes(const HeapWalkEntry& entry);

std::uint64_t HeapWalkCaptureBytes(const ShaderRecompiler::ResourceCapture& capture);

std::uint64_t HeapWalkResultBytes(const ShaderRecompiler::RecompileResult& result);

std::uint64_t HeapWalkWordsBytes(const ShaderRecompiler::HeapStageWords& words);

std::uint64_t HeapWalkMemoBudget();

void NoteHeapWalkMemoEviction();

std::uint64_t HeapWalkMemoEvictions();

struct HeapWalkMemo {
    struct Slot {
        std::vector<HeapWalkEntry> ways;
        std::list<std::uint64_t>::iterator age;
    };
    std::unordered_map<std::uint64_t, Slot> entries;
    std::list<std::uint64_t> order;
    std::vector<std::uint64_t> seen = std::vector<std::uint64_t>(HeapWalkSeenKeys, 0);
    std::unordered_map<const void*, std::pair<std::uint32_t, std::uint64_t>> pins;
    std::vector<HeapWalkVariantUse> variants = std::vector<HeapWalkVariantUse>(HeapWalkVariantSlots);
    std::uint64_t heldBytes = 0;
    std::uint64_t stores = 0;
    std::uint64_t budget = HeapWalkMemoBudget();

    bool consult(std::uint64_t variant) {
        auto& use = variantUse(variant);
        if (use.skip == 0) return true;
        --use.skip;
        return false;
    }

    void noteSkipped(std::uint64_t variant, std::uint64_t key) {
        const bool repeated = admit(key);
        auto& use = variantUse(variant);
        ++use.skipped;
        if (repeated) ++use.repeats;
        if (use.skipped < HeapWalkProbeLookups) return;
        if (use.repeats * 8u >= use.skipped) use.skip = 0;
        use.skipped = 0;
        use.repeats = 0;
    }

    void noteLookup(std::uint64_t variant, bool hit) {
        auto& use = variantUse(variant);
        ++use.lookups;
        if (hit) ++use.hits;
        if (use.lookups < HeapWalkProbeLookups) return;
        if (use.hits == 0u) use.skip = HeapWalkSkippedWalks;
        use.lookups = 0;
        use.hits = 0;
    }

    bool admit(std::uint64_t key) {
        auto& last = seen[(key ^ (key >> 29)) & (HeapWalkSeenKeys - 1u)];
        if (last == key) return true;
        last = key;
        return false;
    }

    std::vector<HeapWalkEntry>* find(std::uint64_t key) {
        const auto found = entries.find(key);
        if (found == entries.end()) return nullptr;
        order.splice(order.begin(), order, found->second.age);
        return &found->second.ways;
    }

    void remember(std::uint64_t key, HeapWalkEntry entry) {
        auto [found, inserted] = entries.try_emplace(key);
        auto& slot = found->second;
        if (inserted) {
            order.push_front(key);
            slot.age = order.begin();
        } else {
            order.splice(order.begin(), order, slot.age);
        }
        if (slot.ways.size() >= HeapWalkWays) {
            drop(slot.ways.back());
            slot.ways.pop_back();
        }
        charge(entry);
        touch(entry);
        slot.ways.insert(slot.ways.begin(), std::move(entry));
        while (entries.size() > HeapWalkMemoKeys) evictOldest();
        if (budget != 0 && heldBytes > budget) trim();
    }

    void touch(HeapWalkEntry& entry) {
        entry.stored = ++stores;
    }

private:
    HeapWalkVariantUse& variantUse(std::uint64_t variant) {
        auto& use = variants[(variant ^ (variant >> 31u)) & (HeapWalkVariantSlots - 1u)];
        if (use.variant != variant) use = {variant};
        return use;
    }

    template <typename TObject, typename TSize>
    void pin(const std::shared_ptr<const TObject>& object, TSize size) {
        if (object == nullptr) return;
        auto& held = pins[object.get()];
        if (held.first++ == 0) {
            held.second = size(*object);
            heldBytes += held.second;
        }
    }

    void unpin(const void* object) {
        if (object == nullptr) return;
        const auto held = pins.find(object);
        if (--held->second.first != 0) return;
        heldBytes -= held->second.second;
        pins.erase(held);
    }

    void charge(HeapWalkEntry& entry) {
        entry.bytes = HeapWalkEntryOwnBytes(entry);
        heldBytes += entry.bytes;
        pin(entry.capture, HeapWalkCaptureBytes);
        pin(entry.result, HeapWalkResultBytes);
        pin(entry.words, HeapWalkWordsBytes);
    }

    void drop(const HeapWalkEntry& entry) {
        heldBytes -= entry.bytes;
        unpin(entry.capture.get());
        unpin(entry.result.get());
        unpin(entry.words.get());
    }

    void evictOldest() {
        const auto key = order.back();
        const auto found = entries.find(key);
        for (const auto& entry : found->second.ways) drop(entry);
        entries.erase(found);
        order.pop_back();
        NoteHeapWalkMemoEviction();
    }

    void trim();
};

}

#endif
