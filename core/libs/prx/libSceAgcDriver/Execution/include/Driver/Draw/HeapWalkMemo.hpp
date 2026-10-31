#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_HEAPWALKMEMO_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_HEAPWALKMEMO_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include <cstddef>
#include <cstdint>
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
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    std::vector<std::uint32_t> values;
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

constexpr std::size_t HeapWalkDrain = 2;

constexpr std::size_t HeapWalkSeenKeys = 1u << 16;

std::uint64_t HeapWalkEntryBytes(const HeapWalkEntry& entry);

std::uint64_t HeapWalkEntryOwnBytes(const HeapWalkEntry& entry);

std::uint64_t HeapWalkCaptureBytes(const ShaderRecompiler::ResourceCapture& capture);

std::uint64_t HeapWalkResultBytes(const ShaderRecompiler::RecompileResult& result);

std::uint64_t HeapWalkWordsBytes(const ShaderRecompiler::HeapStageWords& words);

std::uint64_t HeapWalkMemoBudget();

struct HeapWalkMemo {
    std::unordered_map<std::uint64_t, std::vector<HeapWalkEntry>> current;
    std::unordered_map<std::uint64_t, std::vector<HeapWalkEntry>> previous;
    std::vector<std::uint64_t> seen = std::vector<std::uint64_t>(HeapWalkSeenKeys, 0);
    std::unordered_map<const void*, std::pair<std::uint32_t, std::uint64_t>> pins;
    std::uint64_t heldBytes = 0;
    std::uint64_t stores = 0;
    std::uint64_t budget = HeapWalkMemoBudget();

    bool admit(std::uint64_t key) {
        auto& last = seen[(key ^ (key >> 29)) & (HeapWalkSeenKeys - 1u)];
        if (last == key) return true;
        last = key;
        return false;
    }

    std::vector<HeapWalkEntry>* find(std::uint64_t key) {
        if (const auto found = current.find(key); found != current.end()) return &found->second;
        const auto old = previous.find(key);
        if (old == previous.end()) return nullptr;
        auto moved = std::move(old->second);
        previous.erase(old);
        auto& ways = slot(key);
        ways = std::move(moved);
        return &ways;
    }

    std::vector<HeapWalkEntry>& slot(std::uint64_t key) {
        for (std::size_t drained = 0; drained < HeapWalkDrain && !previous.empty(); ++drained) {
            release(previous.begin()->second);
            previous.erase(previous.begin());
        }
        if (current.size() >= HeapWalkMemoKeys && !current.contains(key)) {
            for (const auto& [old, ways] : previous) release(ways);
            previous = std::move(current);
            current.clear();
        }
        return current[key];
    }

    void remember(std::uint64_t key, HeapWalkEntry entry) {
        auto& ways = slot(key);
        if (ways.size() >= HeapWalkWays) {
            drop(ways.back());
            ways.pop_back();
        }
        charge(entry);
        entry.stored = ++stores;
        ways.insert(ways.begin(), std::move(entry));
        if (budget != 0 && heldBytes > budget) trim();
    }

    void touch(HeapWalkEntry& entry) {
        entry.stored = ++stores;
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

    void release(const std::vector<HeapWalkEntry>& ways) {
        for (const auto& entry : ways) drop(entry);
    }

    void trim();
};

}

#endif
