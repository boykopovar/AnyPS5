#ifndef CORE_SHADER_RECOMPILER_COMPILEDVARIANT_HPP
#define CORE_SHADER_RECOMPILER_COMPILEDVARIANT_HPP

#include "Recompiler.hpp"
#include "IntermediateRepresentation/IrMetadata/CompiledShaderInfo.hpp"
#include "Optimization/include/Optimization/BindingAllocator.hpp"
#include "Optimization/include/Optimization/DescriptorBindingBuilder.hpp"
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

class BufferSubstituteTable {
public:
    explicit BufferSubstituteTable(std::vector<BufferSubstitute> slots) : slots(std::move(slots)), learned(this->slots.size()) {
        for (std::size_t slot = 0; slot < this->slots.size(); ++slot) learned[slot].store(this->slots[slot].known, std::memory_order_relaxed);
    }

    [[nodiscard]] std::vector<BufferSubstitute> Slots() const {
        std::lock_guard lock(mutex);
        return slots;
    }

    void LearnedSlots(std::vector<BufferSubstitute>& out) const {
        out.resize(slots.size());
        for (std::size_t slot = 0; slot < slots.size(); ++slot) out[slot] = learned[slot].load(std::memory_order_acquire) ? slots[slot] : BufferSubstitute{};
    }

    void Learn(std::size_t slot, const BufferSubstitute& value) {
        std::lock_guard lock(mutex);
        if (slot >= slots.size() || slots[slot].known) return;
        slots[slot] = value;
        slots[slot].known = true;
        learned[slot].store(true, std::memory_order_release);
    }

private:
    mutable std::mutex mutex;
    std::vector<BufferSubstitute> slots;
    std::vector<std::atomic<bool>> learned;
};

struct CompiledVariant {
    BindingLayout layout;
    CompiledShaderInfo info;
    CompiledBindingLayout bindings;
    CompiledShaderArtifact artifact;
    std::shared_ptr<BufferSubstituteTable> substitutes;
};

}

#endif
