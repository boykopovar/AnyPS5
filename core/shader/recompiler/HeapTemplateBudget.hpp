#pragma once

#include "Recompiler.hpp"

#include <cstdint>
#include <list>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

template<typename TValue>
[[nodiscard]] std::uint64_t OwnedBytes(const std::vector<TValue>& values) {
    return static_cast<std::uint64_t>(values.capacity()) * sizeof(TValue);
}

[[nodiscard]] inline std::uint64_t OwnedBytes(const std::vector<bool>& values) {
    return (static_cast<std::uint64_t>(values.capacity()) + 7u) / 8u;
}

[[nodiscard]] inline std::uint64_t OwnedBytes(const DescriptorBinding& binding) {
    return OwnedBytes(binding.guestDescriptor) + OwnedBytes(binding.samplerDepthCompare) + OwnedBytes(binding.imageWritten) + OwnedBytes(binding.imageDepthCompare) + OwnedBytes(binding.imageAtomic) + OwnedBytes(binding.imageAtomic64) +
           OwnedBytes(binding.imageShapes) + OwnedBytes(binding.bufferAtomic) + OwnedBytes(binding.bufferWritten) + OwnedBytes(binding.samplerUnnormalized) + OwnedBytes(binding.samplerIntegerBorder) +
           OwnedBytes(binding.samplerCoordinatesIgnored) + OwnedBytes(binding.imageUnnormalized) + OwnedBytes(binding.imageUnnormalizedBaseLevel) + OwnedBytes(binding.imageSamplers) + OwnedBytes(binding.bufferRead) +
           OwnedBytes(binding.bufferExtent);
}

// The bytes a heap template's result owns. Its SPIR-V module is shared with the variant it was
// materialized from, so it is not charged.
[[nodiscard]] inline std::uint64_t OwnedBytes(const RecompileResult& result) {
    std::uint64_t bytes = sizeof(RecompileResult) + OwnedBytes(result.vertexInputPatches) + OwnedBytes(result.runtimeImageResources) + OwnedBytes(result.vertexInputs) + OwnedBytes(result.parameterExports) +
                          OwnedBytes(result.fragmentParameters) + OwnedBytes(result.stencilReferences) + OwnedBytes(result.specialization) + OwnedBytes(result.heapSlotWords) + OwnedBytes(result.bindings) +
                          OwnedBytes(result.pushConstants) + OwnedBytes(result.vertexAttributes) + OwnedBytes(result.imageTablePoison) + OwnedBytes(result.imageTableRanges) + OwnedBytes(result.imageTableReadRanges) +
                          OwnedBytes(result.heapUserWords) + OwnedBytes(result.shaderDataUserWords);
    if (result.meshBarycentrics.has_value()) bytes += OwnedBytes(result.meshBarycentrics->parameters);
    for (const auto& binding : result.bindings) bytes += OwnedBytes(binding);
    for (const auto& ranges : result.imageTableReadRanges) bytes += OwnedBytes(ranges);
    return bytes;
}

// Least recently used order of the heap templates of every shader, under one byte budget. A
// template is named by its owner (the shader it belongs to), its key in that shader's map, and
// the id of the template built for that key, so a touch or an eviction never reaches a later
// template that took the same owner and key.
class HeapTemplateLru {
public:
    struct Name {
        std::uint64_t owner = 0;
        std::uint64_t key = 0;
        std::uint64_t id = 0;
    };

    struct Evicted {
        Name name;
        std::weak_ptr<void> owner;
    };

    explicit HeapTemplateLru(std::uint64_t budget) : budget(budget) {}

    // Adds a template as the most recently used, and returns the templates evicted to bring the
    // total back under the budget, least recently used first.
    [[nodiscard]] std::vector<Evicted> Insert(const Name& name, std::uint64_t bytes, std::weak_ptr<void> owner = {}) {
        std::vector<Evicted> evicted;
        if (const auto found = index.find(Slot{name.owner, name.key}); found != index.end()) {
            total -= found->second->bytes;
            order.erase(found->second);
            index.erase(found);
        }
        order.push_back({name, bytes, std::move(owner)});
        index.emplace(Slot{name.owner, name.key}, std::prev(order.end()));
        total += bytes;
        while (total > budget && !order.empty()) {
            auto oldest = std::move(order.front());
            index.erase(Slot{oldest.name.owner, oldest.name.key});
            order.pop_front();
            total -= oldest.bytes;
            evicted.push_back({oldest.name, std::move(oldest.owner)});
        }
        return evicted;
    }

    void Touch(const Name& name) {
        const auto found = index.find(Slot{name.owner, name.key});
        if (found == index.end() || found->second->name.id != name.id) return;
        order.splice(order.end(), order, found->second);
    }

    [[nodiscard]] std::uint64_t Entries() const { return order.size(); }
    [[nodiscard]] std::uint64_t Bytes() const { return total; }

private:
    struct Entry {
        Name name;
        std::uint64_t bytes = 0;
        std::weak_ptr<void> owner;
    };
    struct Slot {
        std::uint64_t owner = 0;
        std::uint64_t key = 0;
        bool operator==(const Slot&) const = default;
    };
    struct SlotHash {
        std::size_t operator()(const Slot& slot) const { return std::hash<std::uint64_t>{}(slot.owner * 0x9e3779b97f4a7c15ull ^ slot.key); }
    };

    std::uint64_t budget = 0;
    std::uint64_t total = 0;
    std::list<Entry> order;
    std::unordered_map<Slot, std::list<Entry>::iterator, SlotHash> index;
};

}
